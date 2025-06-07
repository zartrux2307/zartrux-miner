

#include <new>
#include "crypto/randomx/allocator.hpp"
#include "crypto/randomx/intrin_portable.h"
#include "crypto/randomx/virtual_memory.hpp"
#include "crypto/randomx/common.hpp"

namespace randomx {

	template<size_t alignment>
	void* AlignedAllocator<alignment>::allocMemory(size_t count) {
		void *mem = rx_aligned_alloc(count, alignment);
		if (mem == nullptr)
			throw std::bad_alloc();
		return mem;
	}

	template<size_t alignment>
	void AlignedAllocator<alignment>::freeMemory(void* ptr, size_t) {
		rx_aligned_free(ptr);
	}

	template struct AlignedAllocator<CacheLineSize>;

	void* LargePageAllocator::allocMemory(size_t count) {
		return allocLargePagesMemory(count);
	}

	void LargePageAllocator::freeMemory(void* ptr, size_t count) {
		freePagedMemory(ptr, count);
	};

}
