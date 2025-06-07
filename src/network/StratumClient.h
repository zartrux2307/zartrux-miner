#ifndef ZARTRUX_SMART_CACHE_H
#define ZARTRUX_SMART_CACHE_H

#include <deque>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <span>
#include <functional>
#include <vector>
#include <memory>
/**
 * @brief SmartCache: FIFO cache para buffers auxiliares de minería Monero.
 * 
 * @note *No se usa para almacenar trabajos o bloques Monero XMR, sino datos
 * temporales (ej: blobs RandomX, buffers de nonces IA, etc).*
 * 100% seguro para minería real de Monero.
 */
namespace zartrux::memory {

class SmartCache {
public:
    static constexpr size_t DefaultCacheSize = 1024;
    static constexpr size_t MaxCacheSize = 1024 * 1024;

    explicit SmartCache(size_t windowSize = DefaultCacheSize);

    SmartCache(const SmartCache&) = delete;
    SmartCache& operator=(const SmartCache&) = delete;

    // Añade callback para alertas métricas (ejemplo: hits/miss)
    void setMetricCallback(std::function<void(size_t, size_t)> cb);

    size_t prefetch(std::span<const uint8_t> data) noexcept;
    std::deque<uint8_t> get_data() const;
    size_t size() const noexcept;
    void resize(size_t newSize);
    void clear();

    size_t get_hit_count() const noexcept;
    size_t get_miss_count() const noexcept;
    void reset_counters() noexcept;

    void debug_print() const noexcept;

    // Permite snapshot para debug
    std::vector<uint8_t> snapshot() const;

private:
    void evict_old_entries(size_t incomingSize);

    std::deque<uint8_t> m_window;
    mutable std::mutex m_mutex;
    std::atomic<size_t> m_hit_count{0};
    std::atomic<size_t> m_miss_count{0};
    std::function<void(size_t, size_t)> metric_callback = nullptr;
};

} // namespace zartrux::memory

#endif // ZARTRUX_SMART_CACHE_H
