
#pragma once

#include <cstdint>
#include <vector>
#include <stdexcept>
#include "crypto/randomx/common.hpp"
#include "crypto/randomx/jit_compiler_a64_static.hpp"

namespace randomx {

	class Program;
	struct ProgramConfiguration;
	class SuperscalarProgram;
	class Instruction;

	typedef void(JitCompilerA64::*InstructionGeneratorA64)(Instruction&, uint32_t&);

	class JitCompilerA64 {
	public:
		explicit JitCompilerA64(bool hugePagesEnable, bool optimizedInitDatasetEnable);
		~JitCompilerA64();

		void prepare() {}
		void generateProgram(Program&, ProgramConfiguration&, uint32_t);
		void generateProgramLight(Program&, ProgramConfiguration&, uint32_t);

		template<size_t N>
		void generateSuperscalarHash(SuperscalarProgram(&programs)[N]);

		void generateDatasetInitCode() {}

		inline ProgramFunc *getProgramFunc() const {
#			ifdef XMRIG_SECURE_JIT
			enableExecution();
#			endif

			return reinterpret_cast<ProgramFunc*>(code);
		}

		DatasetInitFunc* getDatasetInitFunc() const;
		uint8_t* getCode() { return code; }
		size_t getCodeSize();

		void enableWriting() const;
		void enableExecution() const;

		static InstructionGeneratorA64 engine[256];

	private:
		const bool hugePages;
		uint32_t reg_changed_offset[8]{};
		uint8_t* code = nullptr;
		uint32_t literalPos;
		uint32_t num32bitLiterals = 0;
		size_t allocatedSize = 0;

		void allocate(size_t size);

		static void emit32(uint32_t val, uint8_t* code, uint32_t& codePos)
		{
			*(uint32_t*)(code + codePos) = val;
			codePos += sizeof(val);
		}

		static void emit64(uint64_t val, uint8_t* code, uint32_t& codePos)
		{
			*(uint64_t*)(code + codePos) = val;
			codePos += sizeof(val);
		}

		void emitMovImmediate(uint32_t dst, uint32_t imm, uint8_t* code, uint32_t& codePos);
		void emitAddImmediate(uint32_t dst, uint32_t src, uint32_t imm, uint8_t* code, uint32_t& codePos);

		template<uint32_t tmp_reg>
		void emitMemLoad(uint32_t dst, uint32_t src, Instruction& instr, uint8_t* code, uint32_t& codePos);

		template<uint32_t tmp_reg_fp>
		void emitMemLoadFP(uint32_t src, Instruction& instr, uint8_t* code, uint32_t& codePos);

	public:
		void h_IADD_RS(Instruction&, uint32_t&);
		void h_IADD_M(Instruction&, uint32_t&);
		void h_ISUB_R(Instruction&, uint32_t&);
		void h_ISUB_M(Instruction&, uint32_t&);
		void h_IMUL_R(Instruction&, uint32_t&);
		void h_IMUL_M(Instruction&, uint32_t&);
		void h_IMULH_R(Instruction&, uint32_t&);
		void h_IMULH_M(Instruction&, uint32_t&);
		void h_ISMULH_R(Instruction&, uint32_t&);
		void h_ISMULH_M(Instruction&, uint32_t&);
		void h_IMUL_RCP(Instruction&, uint32_t&);
		void h_INEG_R(Instruction&, uint32_t&);
		void h_IXOR_R(Instruction&, uint32_t&);
		void h_IXOR_M(Instruction&, uint32_t&);
		void h_IROR_R(Instruction&, uint32_t&);
		void h_IROL_R(Instruction&, uint32_t&);
		void h_ISWAP_R(Instruction&, uint32_t&);
		void h_FSWAP_R(Instruction&, uint32_t&);
		void h_FADD_R(Instruction&, uint32_t&);
		void h_FADD_M(Instruction&, uint32_t&);
		void h_FSUB_R(Instruction&, uint32_t&);
		void h_FSUB_M(Instruction&, uint32_t&);
		void h_FSCAL_R(Instruction&, uint32_t&);
		void h_FMUL_R(Instruction&, uint32_t&);
		void h_FDIV_M(Instruction&, uint32_t&);
		void h_FSQRT_R(Instruction&, uint32_t&);
		void h_CBRANCH(Instruction&, uint32_t&);
		void h_CFROUND(Instruction&, uint32_t&);
		void h_ISTORE(Instruction&, uint32_t&);
		void h_NOP(Instruction&, uint32_t&);
	};
}
