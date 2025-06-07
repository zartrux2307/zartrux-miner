

#pragma once

#include <cstddef>

namespace randomx {

	template<size_t alignment>
	struct AlignedAllocator {
		static void* allocMemory(size_t);
		static void freeMemory(void*, size_t);
	};

	struct LargePageAllocator {
		static void* allocMemory(size_t);
		static void freeMemory(void*, size_t);
	};

	struct OneGbPageAllocator {
		static void* allocMemory(size_t);
		static void freeMemory(void*, size_t);
	};

}