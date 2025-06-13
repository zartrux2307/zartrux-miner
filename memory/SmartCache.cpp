#include "SmartCache.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <span>
#include <chrono>
#include <cstdlib>  
namespace zartrux::memory {

SmartCache::SmartCache(size_t windowSize) {
    if (windowSize == 0 || windowSize > MaxCacheSize) {
        throw std::invalid_argument("❌ SmartCache window size must be > 0 and <= MaxCacheSize.");
    }
    m_window.resize(windowSize);
}

size_t SmartCache::prefetch(std::span<const uint8_t> data) noexcept {
    if (data.empty()) {
        std::cerr << "⚠️ Warning: Empty data passed to prefetch." << std::endl;
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    size_t copySize = std::min(data.size(), m_window.size());

    evict_old_entries(copySize);

    std::copy(data.begin(), data.begin() + copySize, m_window.begin());

    if (copySize == data.size()) {
        m_hit_count.fetch_add(1, std::memory_order_relaxed);
    } else {
        m_miss_count.fetch_add(1, std::memory_order_relaxed);
    }

    return copySize;
}

void SmartCache::evict_old_entries(size_t incomingSize) {
    if (incomingSize >= m_window.size()) {
        std::fill(m_window.begin(), m_window.end(), 0);
        return;
    }

    std::rotate(m_window.begin(), m_window.begin() + incomingSize, m_window.end());
    std::fill(m_window.end() - incomingSize, m_window.end(), 0);
}

std::deque<uint8_t> SmartCache::get_data() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_window;
}

size_t SmartCache::size() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_window.size();
}

void SmartCache::resize(size_t newSize) {
    if (newSize == 0 || newSize > MaxCacheSize) {
        throw std::invalid_argument("❌ New SmartCache size must be > 0 and <= MaxCacheSize.");
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_window.resize(newSize);
}

void SmartCache::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::fill(m_window.begin(), m_window.end(), 0);
}

size_t SmartCache::get_hit_count() const noexcept {
    return m_hit_count.load(std::memory_order_acquire);
}

size_t SmartCache::get_miss_count() const noexcept {
    return m_miss_count.load(std::memory_order_acquire);
}

void SmartCache::reset_counters() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hit_count.store(0, std::memory_order_release);
    m_miss_count.store(0, std::memory_order_release);
}

void SmartCache::debug_print() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << "[SmartCache Debug]" << std::endl;
    std::cout << "  Size:       " << m_window.size() << std::endl;
    std::cout << "  Hits:       " << m_hit_count.load() << std::endl;
    std::cout << "  Misses:     " << m_miss_count.load() << std::endl;
}

} // namespace zartrux::memory