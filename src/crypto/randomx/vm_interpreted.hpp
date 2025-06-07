

#pragma once

#include <new>
#include <vector>
#include "common.hpp"
#include "crypto/randomx/virtual_machine.hpp"
#include "crypto/randomx/bytecode_machine.hpp"
#include "crypto/randomx/intrin_portable.h"
#include "crypto/randomx/allocator.hpp"

namespace randomx {

	template<int softAes>
	class InterpretedVm : public VmBase<softAes>, public BytecodeMachine {
	public:
		using VmBase<softAes>::mem;
		using VmBase<softAes>::scratchpad;
		using VmBase<softAes>::program;
		using VmBase<softAes>::config;
		using VmBase<softAes>::reg;
		using VmBase<softAes>::datasetPtr;
		using VmBase<softAes>::datasetOffset;

		void* operator new(size_t, void* ptr) { return ptr; }
		void operator delete(void*) {}

		void run(void* seed) override;
		void setDataset(randomx_dataset* dataset) override;

	protected:
		virtual void datasetRead(uint64_t blockNumber, int_reg_t(&r)[RegistersCount]);
		virtual void datasetPrefetch(uint64_t blockNumber);

	private:
		void execute();

		InstructionByteCode bytecode[RANDOMX_PROGRAM_MAX_SIZE];
	};

	using InterpretedVmDefault = InterpretedVm<1>;
	using InterpretedVmHardAes = InterpretedVm<0>;
}
