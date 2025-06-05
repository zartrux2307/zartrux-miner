#include "kernel_x86_avx.h"
#include <immintrin.h>
#include <intrin.h>  // Incluir para __cpuidex en MSVC
#include <cstring>
#include <iostream>

namespace miner {
namespace avx {

namespace {
    alignas(32) constexpr size_t VECTOR_SIZE = 32; // 256 bits (32 bytes)
}

void compute_hash(const uint8_t* input, uint8_t* output, size_t len) {
    if (!input || !output || len == 0) {
        std::cerr << "[compute_hash] Entrada inválida, hash no calculado.\n";
        return;
    }

    __m256i acc = _mm256_setzero_si256();
    size_t processed = 0;

    // Procesar bloques de 32 bytes
    for (; processed + VECTOR_SIZE <= len; processed += VECTOR_SIZE) {
        _mm_prefetch(reinterpret_cast<const char*>(input + processed + 64), _MM_HINT_T0);
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + processed));
        acc = _mm256_xor_si256(acc, chunk);
    }

    // Procesar bytes restantes
    if (processed < len) {
        alignas(32) uint8_t buffer[VECTOR_SIZE] = {0};
        std::memcpy(buffer, input + processed, len - processed);
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(buffer));
        acc = _mm256_xor_si256(acc, chunk);
    }

    // Reducción manual (sustituye a _mm256_reduce_xor_epi32)
    __m256i reduced = _mm256_xor_si256(
        acc,
        _mm256_shuffle_epi32(acc, _MM_SHUFFLE(1, 0, 3, 2)) // Shuffle entre lanes
    );
    reduced = _mm256_xor_si256(reduced, _mm256_srli_si256(reduced, 8));
    reduced = _mm256_xor_si256(reduced, _mm256_srli_si256(reduced, 4));

    // Almacenar los 128 bits más bajos
    _mm_storeu_si128(reinterpret_cast<__m128i*>(output), _mm256_castsi256_si128(reduced));
}

bool is_supported() {
    static bool supported = []() {
        int info[4];

        // Verificar AVX (Leaf 1, bit 28 de ECX)
        __cpuidex(info, 1, 0);
        bool avx = (info[2] & (1 << 28)) != 0;

        // Verificar AVX2 (Leaf 7, bit 5 de EBX)
        __cpuidex(info, 7, 0);
        bool avx2 = (info[1] & (1 << 5)) != 0;

        return avx || avx2;
    }();
    return supported;
}

} // namespace avx
} // namespace miner