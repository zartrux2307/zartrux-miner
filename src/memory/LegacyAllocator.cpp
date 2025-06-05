#include "LegacyAllocator.h"
#include <cstdlib>
#include <new>
#include <iostream>
#include <stdexcept>

namespace zartrux::memory {

std::atomic<size_t> LegacyAllocator::s_total_allocated{0};

void* LegacyAllocator::allocate(size_t size, size_t alignment) noexcept {
    if (size == 0) return nullptr; // No lanzar excepciones (noexcept)

    void* ptr = nullptr;

#ifdef _WIN32
    ptr = _aligned_malloc(size, alignment);
#else
    if (posix_memalign(&ptr, alignment, size) != 0) {
        ptr = nullptr;
    }
#endif

    if (!ptr) {
        std::cerr << "❌ Error: Fallo en la asignación de memoria." << std::endl;
        return nullptr;
    }

    s_total_allocated.fetch_add(size, std::memory_order_relaxed);
    return ptr;
}

void LegacyAllocator::deallocate(void* ptr) noexcept {
    if (!ptr) return;

#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

size_t LegacyAllocator::total_allocated() noexcept {
    return s_total_allocated.load(std::memory_order_relaxed);
}

} // namespace zartrux::memory