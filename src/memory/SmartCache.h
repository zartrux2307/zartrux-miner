#ifndef ZARTRUX_SMART_CACHE_H
#define ZARTRUX_SMART_CACHE_H

#include <deque>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <span>

namespace zartrux::memory {

/**
 * @brief Thread-safe FIFO cache optimized for mining (RandomX/CPU only).
 * @details Estructura eficiente para minado real, solo para uso ético y didáctico en proyectos XMR.
 */
class SmartCache {
public:
    static constexpr size_t DefaultCacheSize = 1024;
    static constexpr size_t MaxCacheSize = 1024 * 1024;

    explicit SmartCache(size_t windowSize = DefaultCacheSize);

    SmartCache(const SmartCache&) = delete;
    SmartCache& operator=(const SmartCache&) = delete;

    // Añade un rango de datos al cache, retorna tamaño real insertado
    size_t prefetch(std::span<const uint8_t> data) noexcept;
    // Devuelve copia local del buffer actual (no para uso intensivo)
    std::deque<uint8_t> get_data() const;
    size_t size() const noexcept;
    void resize(size_t newSize);
    void clear();

    size_t get_hit_count() const noexcept;
    size_t get_miss_count() const noexcept;
    void reset_counters() noexcept;

    // Debug info detallado (para logs/desarrollo)
    void debug_print() const noexcept;

private:
    void evict_old_entries(size_t incomingSize);

    std::deque<uint8_t> m_window;
    mutable std::mutex m_mutex;
    std::atomic<size_t> m_hit_count{0};
    std::atomic<size_t> m_miss_count{0};
};

} // namespace zartrux::memory

#endif // ZARTRUX_SMART_CACHE_H
