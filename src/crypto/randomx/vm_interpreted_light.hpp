

#pragma once

#include <new>
#include "crypto/randomx/vm_interpreted.hpp"

namespace randomx {

	template<int softAes>
	class InterpretedLightVm : public InterpretedVm<softAes> {
	public:
		using VmBase<softAes>::mem;
		using VmBase<softAes>::cachePtr;

		void* operator new(size_t, void* ptr) { return ptr; }
		void operator delete(void*) {}

		void setDataset(randomx_dataset* dataset) override { }
		void setCache(randomx_cache* cache) override;

	protected:
		void datasetRead(uint64_t address, int_reg_t(&r)[8]) override;
		void datasetPrefetch(uint64_t address) override { }
	};

	using InterpretedLightVmDefault = InterpretedLightVm<1>;
	using InterpretedLightVmHardAes = InterpretedLightVm<0>;
}
