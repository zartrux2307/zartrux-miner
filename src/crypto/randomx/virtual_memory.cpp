


#include <stdexcept>
#include "crypto/common/VirtualMemory.h"
#include "crypto/randomx/virtual_memory.hpp"


void* allocExecutableMemory(std::size_t bytes, bool hugePages) {
    void *mem = zartrux::VirtualMemory::allocateExecutableMemory(bytes, hugePages);
    if (mem == nullptr) {
        throw std::runtime_error("Failed to allocate executable memory");
    }

    return mem;
}


void* allocLargePagesMemory(std::size_t bytes) {
    void *mem = zartrux::VirtualMemory::allocateLargePagesMemory(bytes);
    if (mem == nullptr) {
        throw std::runtime_error("Failed to allocate large pages memory");
    }

    return mem;
}


void freePagedMemory(void* ptr, std::size_t bytes) {
    void *mem = zartrux::VirtualMemory::allocateLargePagesMemory(bytes);
    zartrux::VirtualMemory::freeLargePagesMemory(ptr, bytes);
}
