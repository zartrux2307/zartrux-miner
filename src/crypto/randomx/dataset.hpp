#pragma once

#include <cstdint>
#include <vector>
#include <type_traits>
#include "crypto/randomx/common.hpp"
#include "crypto/randomx/superscalar_program.hpp"
#include "crypto/randomx/allocator.hpp"
#include "crypto/randomx/jit_compiler.hpp" // <--- CORRECCIÓN: Se añadió esta cabecera que faltaba

/* Global scope for C binding */
struct randomx_dataset {
	uint8_t* memory = nullptr;
};

/* Global scope for C binding */
struct randomx_cache {
	uint8_t* memory = nullptr;
	randomx::JitCompiler* jit = nullptr;
	randomx::CacheInitializeFunc* initialize;
	randomx::DatasetInitFunc* datasetInit;
	randomx::SuperscalarProgram programs[RANDOMX_CACHE_MAX_ACCESSES];

	bool isInitialized() const {
		return programs[0].getSize() != 0;
	}
};

//A pointer to a standard-layout struct object points to its initial member
static_assert(std::is_standard_layout<randomx_dataset>(), "randomx_dataset must be a standard-layout struct");
static_assert(std::is_standard_layout<randomx_cache>(), "randomx_cache must be a standard-layout struct");

namespace randomx {

	using DefaultAllocator = AlignedAllocator<CacheLineSize>;

	template<class Allocator>
	void deallocDataset(randomx_dataset* dataset) {
		if (dataset->memory != nullptr)
			Allocator::freeMemory(dataset->memory, RANDOMX_DATASET_MAX_SIZE);
	}

	template<class Allocator>
	void deallocCache(randomx_cache* cache);

	void initCache(randomx_cache*, const void*, size_t);
	void initCacheCompile(randomx_cache*, const void*, size_t);
	void initDatasetItem(randomx_cache* cache, uint8_t* out, uint64_t blockNumber);
	void initDataset(randomx_cache* cache, uint8_t* dataset, uint32_t startBlock, uint32_t endBlock);
}