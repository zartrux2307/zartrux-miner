

#pragma once

#include <new>
#include "crypto/randomx/vm_compiled.hpp"

namespace randomx {

	template<int softAes>
	class CompiledLightVm : public CompiledVm<softAes>
	{
	public:
		void* operator new(size_t, void* ptr) { return ptr; }
		void operator delete(void*) {}

		void setCache(randomx_cache* cache) override;
		void setDataset(randomx_dataset* dataset) override { }
		void run(void* seed) override;

		using CompiledVm<softAes>::mem;
		using CompiledVm<softAes>::compiler;
		using CompiledVm<softAes>::program;
		using CompiledVm<softAes>::config;
		using CompiledVm<softAes>::cachePtr;
		using CompiledVm<softAes>::datasetOffset;
	};

	using CompiledLightVmDefault = CompiledLightVm<1>;
	using CompiledLightVmHardAes = CompiledLightVm<0>;
}
