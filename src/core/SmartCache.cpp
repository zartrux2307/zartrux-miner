#include "SmartCache.h"
#include "utils/Logger.h"
#include <randomx.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <memory>
#include <fmt/format.h>
#include <fmt/pointer.h>

// Alineación requerida para conjuntos de datos AVX-512
constexpr size_t AVX512_ALIGNMENT = 64;

SmartCache& SmartCache::getInstance() {
    static SmartCache instance;
    return instance;
}

SmartCache::SmartCache() 
    : m_nextNonce(1)
    , m_datasetCacheSize(0)
    , m_totalMemory(0)
    , m_cacheHits(0)
    , m_cacheMisses(0) {
    // Inicializar con un nonce basado en tiempo
    using namespace std::chrono;
    m_nextNonce.store(system_clock::now().time_since_epoch().count());
}

SmartCache::~SmartCache() {
    // Liberar todos los buffers activos
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [ptr, buffer] : m_activeBuffers) {
        if (buffer.ptr) {
            std::free(buffer.ptr);
            buffer.ptr = nullptr;
        }
    }
    m_activeBuffers.clear();
    m_availableBuffers.clear();
}

uint64_t SmartCache::allocateNonceRange(size_t count) {
    return m_nextNonce.fetch_add(count, std::memory_order_relaxed);
}

std::shared_ptr<const std::vector<uint8_t>> SmartCache::getDataset(const std::string& seed) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 1. Buscar en caché existente
    for (auto& handle : m_datasetCache) {
        if (handle.seed == seed) {
            if (auto dataset = handle.weak_ref.lock()) {
                m_cacheHits++;
                return dataset;
            }
        }
    }
    
    // 2. Crear nuevo dataset
    size_t datasetSize;
    try {
        datasetSize = randomx_dataset_item_count() * RANDOMX_DATASET_ITEM_SIZE;
    } catch (const std::exception& e) {
        Logger::error("SmartCache", "Error al obtener tamaño de dataset: {}", e.what());
        throw;
    }
    
    // Crear vector con padding para alineación
    auto dataset = std::make_shared<std::vector<uint8_t>>(datasetSize + AVX512_ALIGNMENT);
    
    // Alinear memoria para AVX512
    void* alignedPtr = dataset->data();
    size_t space = dataset->size();
    if (!std::align(AVX512_ALIGNMENT, datasetSize, alignedPtr, space)) {
        Logger::error("SmartCache", "Error al alinear memoria para dataset");
        throw std::runtime_error("Error de alineación de memoria");
    }
    
    // Inicializar dataset
    try {
        randomx_dataset* rxDataset = reinterpret_cast<randomx_dataset*>(alignedPtr);
        randomx_init_dataset(rxDataset, seed.data(), seed.size());
        
        Logger::info("SmartCache", "Dataset creado para seed: {}...", 
                    seed.substr(0, std::min<size_t>(8, seed.length())));
    } catch (const std::exception& e) {
        Logger::error("SmartCache", "Error al inicializar dataset: {}", e.what());
        throw;
    }
    
    // 3. Actualizar caché
    m_datasetCache.push_back({seed, dataset});
    m_cacheMisses++;
    
    // Mantener caché bajo límite (LRU simple)
    const size_t MAX_CACHE_SIZE = 3;
    while (m_datasetCache.size() > MAX_CACHE_SIZE) {
        m_datasetCache.erase(m_datasetCache.begin());
    }
    
    return dataset;
}

void* SmartCache::allocateWorkBuffer(size_t size, size_t alignment) {
    if (size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0) {
        throw std::invalid_argument("Tamaño o alineación inválidos");
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 1. Buscar en buffers disponibles
    for (auto it = m_availableBuffers.begin(); it != m_availableBuffers.end(); ++it) {
        auto bufferIt = m_activeBuffers.find(*it);
        if (bufferIt != m_activeBuffers.end()) {
            WorkBuffer& buffer = bufferIt->second;
            if (buffer.size >= size && buffer.alignment == alignment) {
                void* ptr = buffer.ptr;
                m_availableBuffers.erase(it);
                return ptr;
            }
        }
    }
    
    // 2. Asignar nuevo buffer
    void* ptr = nullptr;
#ifdef _WIN32
    ptr = _aligned_malloc(size, alignment);
#else
    if (posix_memalign(&ptr, alignment, size) != 0) {
        ptr = nullptr;
    }
#endif

    if (!ptr) {
        throw std::bad_alloc();
    }
    
    m_activeBuffers[ptr] = WorkBuffer{ptr, size, alignment};
    m_totalMemory.fetch_add(size, std::memory_order_relaxed);
    
    Logger::debug("SmartCache", "Buffer asignado: {} bytes @ {}", 
                 size, fmt::ptr(ptr));
    return ptr;
}

void SmartCache::freeWorkBuffer(void* ptr) {
    if (!ptr) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_activeBuffers.find(ptr);
    if (it != m_activeBuffers.end()) {
        // Reducir el uso total de memoria
        m_totalMemory.fetch_sub(it->second.size, std::memory_order_relaxed);
        
        // Marcar como disponible para reutilización
        m_availableBuffers.push_back(ptr);
        
        Logger::debug("SmartCache", "Buffer liberado: {} bytes @ {}", 
                     it->second.size, fmt::ptr(ptr));
    } else {
        Logger::warn("SmartCache", "Intento de liberar buffer no gestionado: {}", 
                    fmt::ptr(ptr));
    }
}

size_t SmartCache::getMemoryUsage() const {
    return m_totalMemory.load(std::memory_order_relaxed);
}

double SmartCache::getCacheHitRate() const {
    size_t hits = m_cacheHits.load(std::memory_order_relaxed);
    size_t total = hits + m_cacheMisses.load(std::memory_order_relaxed);
    return total ? (static_cast<double>(hits) * 100.0) / total : 0.0;
}