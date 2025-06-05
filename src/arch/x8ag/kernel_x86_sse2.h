#pragma once

#include <cstdint>
#include <cstddef>
#include <emmintrin.h>  // SSE2
#include <xmmintrin.h>

namespace miner {
namespace sse2 {

// Declaraciones sin implementaciones inline
void compute_hash(const uint8_t* input, uint8_t* output, size_t len);
bool is_supported();

} // namespace sse2
} // namespace miner