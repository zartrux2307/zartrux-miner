#pragma once

#include <cstdint>
#include <cstddef>
#include <immintrin.h>  // AVX-512

namespace miner {
namespace avx512 {

/**
 * @brief Calcula un hash utilizando instrucciones AVX-512 (vectores de 512 bits).
 */
void compute_hash(const uint8_t* input, uint8_t* output, size_t len);

/**
 * @brief Verifica si las instrucciones AVX-512 están soportadas en el procesador actual.
 */
bool is_supported();

} // namespace avx512
} // namespace miner