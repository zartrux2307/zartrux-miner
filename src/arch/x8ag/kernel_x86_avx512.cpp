#include "kernel_x86_avx512.h"
#include <immintrin.h>
#include <intrin.h>  // Para __cpuidex en MSVC
#include <cstring>
#include <iostream>

namespace miner {
namespace avx512 {

namespace {
    alignas(64) constexpr size_t VECTOR_SIZE = 64; // 512 bits (64 bytes)
}

void compute_hash(const uint8_t* input, uint8_t* output, size_t len) {
    if (!input || !output || len == 0) {
        std::cerr << "[compute_hash] Entrada inválida, hash no calculado.\n";
        return;
    }

    __m512i acc = _mm512_setzero_si512(); // Inicializa el acumulador en cero
    size_t processed = 0;

    // Procesar bloques de 64 bytes
    for (; processed + VECTOR_SIZE <= len; processed += VECTOR_SIZE) {
        _mm_prefetch(reinterpret_cast<const char*>(input + processed + 128), _MM_HINT_T0);
        __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(input + processed));
        acc = _mm512_xor_si512(acc, chunk);
    }

    // Procesar bytes restantes
    if (processed < len) {
        alignas(64) uint8_t buffer[VECTOR_SIZE] = {0};
        std::memcpy(buffer, input + processed, len - processed);
        __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(buffer));
        acc = _mm512_xor_si512(acc, chunk);
    }

    // Reducción manual (sustituye a _mm512_reduce_xor_epi64)
    __m512i reduced = _mm512_xor_si512(
        acc,
        _mm512_shuffle_i64x2(acc, acc, _MM_SHUFFLE(1, 0, 3, 2)) // Shuffle entre lanes de 128 bits
    );
    reduced = _mm512_xor_si512(reduced, _mm512_srli_epi64(reduced, 32));
    reduced = _mm512_xor_si512(reduced, _mm512_srli_epi64(reduced, 16));

    // Almacenar los 128 bits más bajos
    _mm_storeu_si128(reinterpret_cast<__m128i*>(output), _mm512_castsi512_si128(reduced));
}

bool is_supported() {
    static bool supported = []() {
        int info[4];

        // Verificar AVX-512 Foundation (Leaf 7, bit 16 de EBX)
        __cpuidex(info, 7, 0);
        bool avx512f = (info[1] & (1 << 16)) != 0;

        if (!avx512f) {
            std::cerr << "[is_supported] AVX-512 no es compatible en este procesador.\n";
        }
        return avx512f;
    }();
    return supported;
}

} // namespace avx512
} // namespace miner