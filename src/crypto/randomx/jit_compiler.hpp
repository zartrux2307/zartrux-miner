
#pragma once

#if defined(XMRIG_FEATURE_ASM) && (defined(_M_X64) || defined(__x86_64__))
#include "crypto/randomx/jit_compiler_x86.hpp"
#elif defined(__aarch64__)
#include "crypto/randomx/jit_compiler_a64.hpp"
#else
#include "crypto/randomx/jit_compiler_fallback.hpp"
#endif
