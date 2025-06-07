

#include "crypto/randomx/vm_compiled.hpp"
#include "crypto/randomx/common.hpp"
#include "crypto/rx/Profiler.h"

namespace randomx {

	static_assert(sizeof(MemoryRegisters) == 2 * sizeof(addr_t) + sizeof(uintptr_t), "Invalid alignment of struct randomx::MemoryRegisters");
	static_assert(sizeof(RegisterFile) == 256, "Invalid alignment of struct randomx::RegisterFile");

	template<int softAes>
	void CompiledVm<softAes>::setDataset(randomx_dataset* dataset) {
		datasetPtr = dataset;
	}

	template<int softAes>
	void CompiledVm<softAes>::run(void* seed) {
		PROFILE_SCOPE(RandomX_run);

		compiler.prepare();
		VmBase<softAes>::generateProgram(seed);
		randomx_vm::initialize();
		compiler.generateProgram(program, config, randomx_vm::getFlags());
		mem.memory = datasetPtr->memory + datasetOffset;
		execute();
	}

	template<int softAes>
	void CompiledVm<softAes>::execute() {
		PROFILE_SCOPE(RandomX_JIT_execute);

#		ifdef XMRIG_ARM
		memcpy(reg.f, config.eMask, sizeof(config.eMask));
#		endif
		compiler.getProgramFunc()(reg, mem, scratchpad, RandomX_CurrentConfig.ProgramIterations);
	}

	template class CompiledVm<false>;
	template class CompiledVm<true>;
}
