

#pragma once

#include <cstdint>
#include "crypto/randomx/instruction.hpp"
#include "crypto/randomx/common.hpp"

namespace randomx {

	class SuperscalarProgram {
	public:
		Instruction& operator()(int pc) {
			return programBuffer[pc];
		}
		uint32_t getSize() const {
			return size;
		}
		void setSize(uint32_t val) {
			size = val;
		}
		int getAddressRegister() const {
			return addrReg;
		}
		void setAddressRegister(int val) {
			addrReg = val;
		}

		Instruction programBuffer[SuperscalarMaxSize];
		uint32_t size
#ifndef NDEBUG
			= 0
#endif
			;
		int addrReg;
		double ipc;
		int codeSize;
		int macroOps;
		int decodeCycles;
		int cpuLatency;
		int asicLatency;
		int mulCount;
		int cpuLatencies[8];
		int asicLatencies[8];
	};

}
