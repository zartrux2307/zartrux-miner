#pragma once

#include <cstdint>
#include <cstddef>
#include <immintrin.h>

namespace miner {
namespace avx {

/**
 * @brief Calcula un hash utilizando instrucciones AVX para procesamiento paralelo.
 */
void compute_hash(const uint8_t* input, uint8_t* output, size_t len);

/**
 * @brief Verifica si las instrucciones AVX/AVX2 son compatibles con la CPU actual.
 */
bool is_supported();

} // namespace avx
} // namespace miner