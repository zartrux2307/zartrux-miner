

#include "crypto/randomx/vm_compiled_light.hpp"
#include "crypto/randomx/common.hpp"
#include <stdexcept>

namespace randomx {

	template<int softAes>
	void CompiledLightVm<softAes>::setCache(randomx_cache* cache) {
		cachePtr = cache;
		mem.memory = cache->memory;

#		ifdef zartrux_SECURE_JIT
		compiler.enableWriting();
#		endif

		compiler.generateSuperscalarHash(cache->programs);
	}

	template<int softAes>
	void CompiledLightVm<softAes>::run(void* seed) {
		VmBase<softAes>::generateProgram(seed);
		randomx_vm::initialize();

#		ifdef zartrux_SECURE_JIT
		compiler.enableWriting();
#		endif

		compiler.generateProgramLight(program, config, datasetOffset);

		CompiledVm<softAes>::execute();
	}

	template class CompiledLightVm<false>;
	template class CompiledLightVm<true>;
}
