#include "hash.h"
#include <stdexcept>
#include <cstring>
#include <thread>
#include <mutex>

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
    if (m_initialized) return; // Idempotente
    m_config = config;
    m_cache = randomx_alloc_cache(config.flags);
    if (!m_cache) throw std::runtime_error("RandomX cache allocation failed");
    randomx_init_cache(m_cache, key.data(), key.size());
    if (config.fullMemory) {
        m_dataset = randomx_alloc_dataset(config.flags);
        if (!m_dataset) throw std::runtime_error("RandomX dataset allocation failed");
        size_t items = randomx_dataset_item_count();
        randomx_init_dataset(m_dataset, m_cache, 0, items);
    }
    m_initialized = true;
}

void RandomXContext::cleanup() { destroy(); }

void RandomXContext::destroy() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_dataset) { randomx_release_dataset(m_dataset); m_dataset = nullptr; }
    if (m_cache)   { randomx_release_cache(m_cache); m_cache = nullptr; }
    m_initialized = false;
}

randomx_dataset* RandomXContext::dataset() { return m_dataset; }
randomx_cache* RandomXContext::cache() { return m_cache; }
const RandomXConfig& RandomXContext::getConfig() const { return m_config; }

// --- RandomXVM ----

RandomXVM::RandomXVM(const RandomXConfig& config) {
    auto& ctx = RandomXContext::getInstance();
    if (!ctx.cache()) throw std::runtime_error("RandomX not initialized");
    if (config.fullMemory) {
        m_vm = randomx_create_vm(config.flags, ctx.cache(), ctx.dataset());
    } else {
        m_vm = randomx_create_vm(config.flags, ctx.cache(), nullptr);
    }
    if (!m_vm) throw std::runtime_error("RandomX VM allocation failed");
}

RandomXVM::~RandomXVM() {
    if (m_vm) randomx_destroy_vm(m_vm);
}

std::array<uint8_t, 32> RandomXVM::calculateHash(std::span<const uint8_t> data) const {
    std::array<uint8_t, 32> hash{};
    if (!m_vm) throw std::runtime_error("RandomX VM not initialized");
    randomx_calculate_hash(m_vm, data.data(), data.size(), hash.data());
    return hash;
}

randomx_vm* RandomXVM::get() { return m_vm; }

// --- API de hashing único ---

std::array<uint8_t, 32> monero_hash(std::span<const uint8_t> data) {
    // Cada hilo tiene su propia VM (thread_local) para máxima eficiencia
    thread_local RandomXVM vm;
    return vm.calculateHash(data);
}

} // namespace core
