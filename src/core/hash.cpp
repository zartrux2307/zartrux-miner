#include "hash.h"
#include "utils/Logger.h" // Se asume que Logger está en utils
#include <stdexcept>
#include <cstring>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>

namespace core {

// --- RandomXContext ----

RandomXContext& RandomXContext::getInstance() {
    static RandomXContext instance;
    return instance;
}

RandomXContext::RandomXContext() = default;
RandomXContext::~RandomXContext() { destroy(); }

void RandomXContext::initialize(const std::vector<uint8_t>& key, const RandomXConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return;

    auto start_time = std::chrono::high_resolution_clock::now();

    m_config = config;

    m_cache = randomx_create_cache(m_config.flags, nullptr);
    if (!m_cache) {
        throw std::runtime_error("Fallo al reservar la caché de RandomX");
    }

    randomx_init_cache(m_cache, key.data(), key.size());

    if (m_config.fullMemory) {
        m_dataset = randomx_create_dataset(nullptr);
        if (!m_dataset) {
            randomx_release_cache(m_cache);
            m_cache = nullptr;
            throw std::runtime_error("Fallo al reservar el dataset de RandomX");
        }

        // --- MEJORA (Punto 4a): Inicialización del dataset en paralelo ---
        unsigned thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) thread_count = 1;
        
        unsigned long items_count = randomx_dataset_item_count();
        unsigned long items_per_thread = items_count / thread_count;
        
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < thread_count; ++i) {
            unsigned long start_item = i * items_per_thread;
            unsigned long count = (i == thread_count - 1) 
                                ? (items_count - start_item) 
                                : items_per_thread;
            
            threads.emplace_back(randomx_init_dataset, m_dataset, m_cache, start_item, count);
        }

        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    // --- MEJORA (Punto 5): Métrica de rendimiento ---
    Logger::info("RandomXContext", "Contexto RandomX inicializado en %lld ms.", duration);
    
    m_initialized = true;
}

void RandomXContext::reinitialize(const std::vector<uint8_t>& key, const RandomXConfig& config) {
    destroy();
    initialize(key, config);
}

void RandomXContext::destroy() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;
    
    if (m_dataset) { randomx_release_dataset(m_dataset); m_dataset = nullptr; }
    if (m_cache)   { randomx_release_cache(m_cache); m_cache = nullptr; }
    m_initialized = false;
}

randomx_dataset* RandomXContext::dataset() { return m_dataset; }
randomx_cache* RandomXContext::cache() { return m_cache; }
const RandomXConfig& RandomXContext::getConfig() const { return m_config; }
bool RandomXContext::isInitialized() const { return m_initialized.load(); }

// --- RandomXVM ----

RandomXVM::RandomXVM(const RandomXConfig& config) {
    auto& ctx = RandomXContext::getInstance();
    if (!ctx.isInitialized()) {
        throw std::runtime_error("El contexto de RandomX no está inicializado para crear una VM");
    }

    m_vm = randomx_create_vm(config.flags, ctx.cache(), config.fullMemory ? ctx.dataset() : nullptr, nullptr, 0);
    if (!m_vm) {
        throw std::runtime_error("Fallo al reservar la VM de RandomX");
    }
}

RandomXVM::~RandomXVM() {
    if (m_vm) randomx_destroy_vm(m_vm);
}

// --- MEJORA (Punto 4b): Implementación de Move Semantics ---
RandomXVM::RandomXVM(RandomXVM&& other) noexcept {
    m_vm = other.m_vm;
    other.m_vm = nullptr; // Evitar doble liberación de memoria
}

RandomXVM& RandomXVM::operator=(RandomXVM&& other) noexcept {
    if (this != &other) {
        if (m_vm) {
            randomx_destroy_vm(m_vm);
        }
        m_vm = other.m_vm;
        other.m_vm = nullptr;
    }
    return *this;
}

std::array<uint8_t, 32> RandomXVM::calculateHash(std::span<const uint8_t> data) const {
    std::array<uint8_t, 32> hash{};
    if (!m_vm) {
        throw std::runtime_error("Intento de hashear con una VM de RandomX no válida (posiblemente movida)");
    }
    randomx_calculate_hash(m_vm, data.data(), data.size(), hash.data());
    return hash;
}

randomx_vm* RandomXVM::get() const {
    return m_vm;
}

} // namespace core