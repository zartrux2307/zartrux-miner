

#pragma once

#include <new>
#include <cstdint>
#include "crypto/randomx/virtual_machine.hpp"
#include "crypto/randomx/jit_compiler.hpp"
#include "crypto/randomx/allocator.hpp"
#include "crypto/randomx/dataset.hpp"

namespace randomx {

	template<int softAes>
	class CompiledVm : public VmBase<softAes>
	{
	public:
		inline CompiledVm() {}
		void* operator new(size_t, void* ptr) { return ptr; }
		void operator delete(void*) {}

		void setDataset(randomx_dataset* dataset) override;
		void run(void* seed) override;

		using VmBase<softAes>::mem;
		using VmBase<softAes>::program;
		using VmBase<softAes>::config;
		using VmBase<softAes>::reg;
		using VmBase<softAes>::scratchpad;
		using VmBase<softAes>::datasetPtr;
		using VmBase<softAes>::datasetOffset;

	protected:
		void execute();

		JitCompiler compiler{ true, false };
	};

	using CompiledVmDefault = CompiledVm<1>;
	using CompiledVmHardAes = CompiledVm<0>;
}
