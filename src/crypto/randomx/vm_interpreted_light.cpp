

#include "crypto/randomx/vm_interpreted_light.hpp"
#include "crypto/randomx/dataset.hpp"

namespace randomx {

	template<int softAes>
	void InterpretedLightVm<softAes>::setCache(randomx_cache* cache) {
		cachePtr = cache;
		mem.memory = cache->memory;
	}

	template<int softAes>
	void InterpretedLightVm<softAes>::datasetRead(uint64_t address, int_reg_t(&r)[8]) {
		uint32_t itemNumber = address / CacheLineSize;
		int_reg_t rl[8];
		
		initDatasetItem(cachePtr, (uint8_t*)rl, itemNumber);

		for (unsigned q = 0; q < 8; ++q)
			r[q] ^= rl[q];
	}

	template class InterpretedLightVm<false>;
	template class InterpretedLightVm<true>;
}
