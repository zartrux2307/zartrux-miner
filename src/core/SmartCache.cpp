#include "SmartCache.h"
#include "utils/Logger.h"
#include "randomx.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cmath>

// Alineación requerida para conjuntos de datos AVX-512
constexpr size_t AVX512_ALIGNMENT = 64;

SmartCache& SmartCache::getInstance() {
    static SmartCache instance;
    return instance;
}

SmartCache::SmartCache() 
    : m_nextNonce(1),
      m_datasetCacheSize(0) {
    // Inicializar con un nonce basado en tiempo
    m_nextNonce.store(std::chrono::system_clock::now().time_since_epoch().count());
}

SmartCache::~SmartCache() {
    // Liberar todos los buffers activos
    for (auto& [ptr, buffer] : m_activeBuffers) {
        std::free(buffer.ptr);
    }
    m_activeBuffers.clear();
    m_availableBuffers.clear();
}

uint64_t SmartCache::allocateNonceRange(size_t count) {
    // Incremento atómico para evitar colisiones
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
    constexpr size_t datasetSize = randomx_dataset_item_count() * sizeof(randomx_dataset_item);
    auto dataset = std::make_shared<std::vector<uint8_t>>(datasetSize + AVX512_ALIGNMENT);
    
    // Alinear memoria para AVX512
    uint8_t* alignedPtr = dataset->data();
    size_t space = dataset->size();
    std::align(AVX512_ALIGNMENT, datasetSize, (void*&)alignedPtr, space);
    
    // Inicializar dataset (operación costosa)
    randomx_dataset_init(reinterpret_cast<randomx_dataset*>(alignedPtr), 
                        seed.data(), seed.size());
    
    Logger::info("Dataset creado para seed: {}", seed.substr(0, 8) + "...");
    
    // 3. Actualizar caché
    m_datasetCache.push_back({seed, dataset});
    m_cacheMisses++;
    
    // Mantener caché bajo límite (LRU simple)
    if (m_datasetCache.size() > 3) {
        m_datasetCache.erase(m_datasetCache.begin());
    }
    
    return dataset;
}

void* SmartCache::allocateWorkBuffer(size_t size, size_t alignment) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 1. Buscar en buffers disponibles
    for (auto it = m_availableBuffers.begin(); it != m_availableBuffers.end(); ++it) {
        WorkBuffer& buffer = m_activeBuffers[*it];
        if (buffer.size >= size && buffer.alignment == alignment) {
            void* ptr = *it;
            m_availableBuffers.erase(it);
            return ptr;
        }
    }
    
    // 2. Asignar nuevo buffer
    void* ptr = std::aligned_alloc(alignment, size);
    if (!ptr) {
        throw std::bad_alloc();
    }
    
    m_activeBuffers[ptr] = {ptr, size, alignment};
    m_totalMemory += size;
    
    Logger::debug("Buffer asignado: {} bytes @ {}", size, fmt::ptr(ptr));
    return ptr;
}

void SmartCache::freeWorkBuffer(void* ptr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_activeBuffers.find(ptr) != m_activeBuffers.end()) {
        // Marcar como disponible para reutilización
        m_availableBuffers.push_back(ptr);
    } else {
        Logger::warn("Intento de liberar buffer no gestionado: {}", fmt::ptr(ptr));
    }
}

size_t SmartCache::getMemoryUsage() const {
    return m_totalMemory.load();
}

size_t SmartCache::getCacheHitRate() const {
    size_t hits = m_cacheHits.load();
    size_t total = hits + m_cacheMisses.load();
    return total ? (hits * 100) / total : 0;
}