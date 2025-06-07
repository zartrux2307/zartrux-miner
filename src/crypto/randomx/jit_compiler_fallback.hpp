

#pragma once

#include <cstdint>
#include <vector>
#include <stdexcept>
#include "crypto/randomx/common.hpp"

namespace randomx {

	class Program;
	class ProgramConfiguration;
	class SuperscalarProgram;

	class JitCompilerFallback {
	public:
		explicit JitCompilerFallback(bool, bool) {
			throw std::runtime_error("JIT compilation is not supported on this platform");
		}
		void prepare() {}
		void generateProgram(Program&, ProgramConfiguration&, uint32_t) {

		}
		void generateProgramLight(Program&, ProgramConfiguration&, uint32_t) {

		}
		template<size_t N>
		void generateSuperscalarHash(SuperscalarProgram(&programs)[N]) {

		}
		void generateDatasetInitCode() {

		}
		ProgramFunc* getProgramFunc() {
			return nullptr;
		}
		DatasetInitFunc* getDatasetInitFunc() {
			return nullptr;
		}
		uint8_t* getCode() {
			return nullptr;
		}
		size_t getCodeSize() {
			return 0;
		}
		void enableWriting() {}
		void enableExecution() {}
	};
}
