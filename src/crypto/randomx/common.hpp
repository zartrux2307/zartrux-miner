

#pragma once

#include <cstdint>
#include <iostream>
#include <climits>
#include "crypto/randomx/blake2/endian.h"
#include "crypto/randomx/configuration.h"
#include "crypto/randomx/randomx.h"

namespace randomx {

	constexpr uint32_t ArgonBlockSize = 1024;
	constexpr int SuperscalarMaxSize = 3 * RANDOMX_SUPERSCALAR_MAX_LATENCY + 2;
	constexpr size_t CacheLineSize = RANDOMX_DATASET_ITEM_SIZE;
	#define ScratchpadSize RandomX_CurrentConfig.ScratchpadL3_Size
	#define CacheLineAlignMask RandomX_ConfigurationBase::CacheLineAlignMask_Calculated
	#define DatasetExtraItems RandomX_ConfigurationBase::DatasetExtraItems_Calculated
	constexpr int StoreL3Condition = 14;

	
#ifndef RANDOMX_UNSAFE
	
#endif

#ifdef TRACE
	constexpr bool trace = true;
#else
	constexpr bool trace = false;
#endif

#ifndef UNREACHABLE
#ifdef __GNUC__
#define UNREACHABLE __builtin_unreachable()
#elif _MSC_VER
#define UNREACHABLE __assume(false)
#else
#define UNREACHABLE
#endif
#endif

#if defined(XMRIG_FEATURE_ASM) && (defined(_M_X64) || defined(__x86_64__))
	#define RANDOMX_HAVE_COMPILER 1
	class JitCompilerX86;
	using JitCompiler = JitCompilerX86;
#elif defined(__aarch64__)
	#define RANDOMX_HAVE_COMPILER 1
	class JitCompilerA64;
	using JitCompiler = JitCompilerA64;
#else
	#define RANDOMX_HAVE_COMPILER 0
	class JitCompilerFallback;
	using JitCompiler = JitCompilerFallback;
#endif

	using addr_t = uint32_t;

	using int_reg_t = uint64_t;

	struct fpu_reg_t {
		double lo;
		double hi;
	};

	#define AddressMask RandomX_CurrentConfig.AddressMask_Calculated
	#define ScratchpadL3Mask RandomX_CurrentConfig.ScratchpadL3Mask_Calculated
	#define ScratchpadL3Mask64 RandomX_CurrentConfig.ScratchpadL3Mask64_Calculated
	constexpr int RegistersCount = 8;
	constexpr int RegisterCountFlt = RegistersCount / 2;
	constexpr int RegisterNeedsDisplacement = 5; //x86 r13 register
	constexpr int RegisterNeedsSib = 4; //x86 r12 register

	inline bool isZeroOrPowerOf2(uint64_t x) {
		return (x & (x - 1)) == 0;
	}

	constexpr int mantissaSize = 52;
	constexpr int exponentSize = 11;
	constexpr uint64_t mantissaMask = (1ULL << mantissaSize) - 1;
	constexpr uint64_t exponentMask = (1ULL << exponentSize) - 1;
	constexpr int exponentBias = 1023;
	constexpr int dynamicExponentBits = 4;
	constexpr int staticExponentBits = 4;
	constexpr uint64_t constExponentBits = 0x300;
	constexpr uint64_t dynamicMantissaMask = (1ULL << (mantissaSize + dynamicExponentBits)) - 1;

	struct MemoryRegisters {
		addr_t mx, ma;
		uint8_t* memory = nullptr;
	};

	//register file in little-endian byte order
	struct RegisterFile {
		int_reg_t r[RegistersCount];
		fpu_reg_t f[RegisterCountFlt];
		fpu_reg_t e[RegisterCountFlt];
		fpu_reg_t a[RegisterCountFlt];
	};

	typedef void(ProgramFunc)(RegisterFile&, MemoryRegisters&, uint8_t* /* scratchpad */, uint64_t);
	typedef void(DatasetInitFunc)(randomx_cache* cache, uint8_t* dataset, uint32_t startBlock, uint32_t endBlock);

	typedef void(CacheInitializeFunc)(randomx_cache*, const void*, size_t);
}
