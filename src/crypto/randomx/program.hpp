

#pragma once

#include <cstdint>
#include "crypto/randomx/common.hpp"
#include "crypto/randomx/instruction.hpp"
#include "crypto/randomx/blake2/endian.h"

namespace randomx {

	struct ProgramConfiguration {
		uint64_t eMask[2];
		uint32_t readReg0, readReg1, readReg2, readReg3;
	};

	class Program {
	public:
		Instruction& operator()(int pc) {
			return programBuffer[pc];
		}
		uint64_t getEntropy(int i) {
			return load64(&entropyBuffer[i]);
		}
		uint32_t getSize() {
			return RandomX_CurrentConfig.ProgramSize;
		}
	private:
		uint64_t entropyBuffer[16];
		Instruction programBuffer[RANDOMX_PROGRAM_MAX_SIZE];
	};

	static_assert(sizeof(Program) % 64 == 0, "Invalid size of class randomx::Program");
}
