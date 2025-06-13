#include "VirtualMemory.h"
#include <stdexcept>

// Incluye las cabeceras específicas de cada sistema operativo
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace zartrux {

#if defined(_WIN32)
// --- Implementación para Windows ---

void* VirtualMemory::allocateExecutableMemory(size_t bytes, bool hugePages) {
    // En Windows, la memoria ejecutable y las páginas grandes se manejan de forma similar.
    // Para simplificar, aquí usamos la protección estándar para memoria ejecutable.
    return VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}

void* VirtualMemory::allocateLargePagesMemory(size_t bytes) {
    // Para usar páginas grandes en Windows, el usuario necesita el privilegio "Bloquear páginas en la memoria".
    size_t largePageSize = GetLargePageMinimum();
    if (largePageSize > 0 && bytes >= largePageSize) {
        size_t alignedBytes = (bytes + largePageSize - 1) / largePageSize * largePageSize;
        void* mem = VirtualAlloc(nullptr, alignedBytes, MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE);
        if (mem) return mem;
    }
    // Si falla o no está disponible, se usa memoria normal.
    return VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void VirtualMemory::freeLargePagesMemory(void* ptr, size_t bytes) {
    if (ptr) {
        VirtualFree(ptr, 0, MEM_RELEASE);
    }
}

void VirtualMemory::protectRX(void* ptr, size_t bytes) {
    DWORD oldProtect;
    VirtualProtect(ptr, bytes, PAGE_EXECUTE_READ, &oldProtect);
}

void VirtualMemory::protectRW(void* ptr, size_t bytes) {
    DWORD oldProtect;
    VirtualProtect(ptr, bytes, PAGE_READWRITE, &oldProtect);
}

#else
// --- Implementación para Linux/macOS ---

void* VirtualMemory::allocateExecutableMemory(size_t bytes, bool hugePages) {
    int prot = PROT_READ | PROT_WRITE | PROT_EXEC;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    #ifdef MAP_HUGETLB
    if (hugePages) {
        flags |= MAP_HUGETLB;
    }
    #endif
    void* mem = mmap(nullptr, bytes, prot, flags, -1, 0);
    return (mem == MAP_FAILED) ? nullptr : mem;
}

void* VirtualMemory::allocateLargePagesMemory(size_t bytes) {
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    #ifdef MAP_HUGETLB
    flags |= MAP_HUGETLB;
    #endif
    void* mem = mmap(nullptr, bytes, prot, flags, -1, 0);
    return (mem == MAP_FAILED) ? nullptr : mem;
}

void VirtualMemory::freeLargePagesMemory(void* ptr, size_t bytes) {
    if (ptr) {
        munmap(ptr, bytes);
    }
}

void VirtualMemory::protectRX(void* ptr, size_t bytes) {
    if (ptr) mprotect(ptr, bytes, PROT_READ | PROT_EXEC);
}

void VirtualMemory::protectRW(void* ptr, size_t bytes) {
    if (ptr) mprotect(ptr, bytes, PROT_READ | PROT_WRITE);
}

#endif

} // namespace zartrux