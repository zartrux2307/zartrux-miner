#ifndef ZARTRUX_LEGACY_ALLOCATOR_H
#define ZARTRUX_LEGACY_ALLOCATOR_H

#include <cstddef>
#include <atomic>

namespace zartrux::memory {

/**
 * @brief Traditional memory allocator using malloc/free with alignment support.
 * @details Provides thread-safe memory allocation with alignment for performance-critical applications.
 *          Keeps track of the total allocated memory.
 */
class LegacyAllocator {
public:
    static constexpr size_t DefaultAlignment = 64;

    /**
     * @brief Allocates an aligned memory block.
     * @param size Size in bytes to allocate.
     * @param alignment Desired alignment in bytes (defaults to 64).
     * @return Pointer to allocated memory or nullptr if allocation fails.
     * @note Thread-safe.
     */
    static void* allocate(size_t size, size_t alignment = DefaultAlignment) noexcept;

    /**
     * @brief Deallocates a memory block.
     * @param ptr Pointer to the memory block to free. If `ptr` is nullptr, no action is taken.
     * @note Thread-safe.
     */
    static void deallocate(void* ptr) noexcept;

    /**
     * @brief Retrieves the total allocated memory.
     * @return Total allocated memory in bytes.
     * @note Thread-safe.
     */
    static size_t total_allocated() noexcept;

private:
    static std::atomic<size_t> s_total_allocated;
};

} // namespace zartrux::memory

#endif // ZARTRUX_LEGACY_ALLOCATOR_H