

#pragma once

#include <stdint.h>

#if defined(_M_X64) || defined(__x86_64__)
#define RANDOMX_HAVE_FAST_RECIPROCAL 1
#else
#define RANDOMX_HAVE_FAST_RECIPROCAL 0
#endif

#if defined(__cplusplus)
extern "C" {
#endif

uint64_t randomx_reciprocal(uint64_t);
uint64_t randomx_reciprocal_fast(uint64_t);

#if defined(__cplusplus)
}
#endif
