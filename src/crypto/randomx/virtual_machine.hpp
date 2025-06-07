

#pragma once

#include <cstdint>
#include "crypto/randomx/common.hpp"
#include "crypto/randomx/program.hpp"

/* Global namespace for C binding */
class randomx_vm
{
public:
	virtual ~randomx_vm() = 0;
	virtual void setScratchpad(uint8_t *scratchpad) = 0;
	virtual void getFinalResult(void* out) = 0;
	virtual void hashAndFill(void* out, uint64_t (&fill_state)[8]) = 0;
	virtual void setDataset(randomx_dataset* dataset) { }
	virtual void setCache(randomx_cache* cache) { }
	virtual void initScratchpad(void* seed) = 0;
	virtual void run(void* seed) = 0;
	void resetRoundingMode();

	void setFlags(uint32_t flags) { vm_flags = flags; }
	uint32_t getFlags() const { return vm_flags; }

	randomx::RegisterFile *getRegisterFile() {
		return &reg;
	}

	const void* getScratchpad() {
		return scratchpad;
	}

	const randomx::Program& getProgram()
	{
		return program;
	}

protected:
	void initialize();
	alignas(64) randomx::Program program;
	alignas(64) randomx::RegisterFile reg;
	alignas(16) randomx::ProgramConfiguration config;
	randomx::MemoryRegisters mem;
	uint8_t* scratchpad = nullptr;
	union {
		randomx_cache* cachePtr = nullptr;
		randomx_dataset* datasetPtr;
	};
	uint64_t datasetOffset;
	uint32_t vm_flags;
};

namespace randomx {

	template<int softAes>
	class VmBase : public randomx_vm
	{
	public:
		~VmBase() override;
		void setScratchpad(uint8_t *scratchpad) override;
		void initScratchpad(void* seed) override;
		void getFinalResult(void* out) override;
		void hashAndFill(void* out, uint64_t (&fill_state)[8]) override;

	protected:
		void generateProgram(void* seed);
	};

}
