#include "kernel_x86_sse2.h"
#include <intrin.h> // Para __cpuidex en MSVC
#include <cstring>
#include <iostream>

namespace miner {
namespace sse2 {

namespace {
    constexpr size_t VECTOR_SIZE = 16; // 128 bits (16 bytes)
}

void compute_hash(const uint8_t* input, uint8_t* output, size_t len) {
    if (!input || !output || len == 0) {
        std::cerr << "[compute_hash] Entrada inválida, hash no calculado.\n";
        return;
    }

    __m128i acc = _mm_setzero_si128(); // Inicializa el acumulador en cero
    size_t processed = 0;

    // Procesar vectores completos con prefetch seguro
    for (; processed + VECTOR_SIZE <= len; processed += VECTOR_SIZE) {
        _mm_prefetch(reinterpret_cast<const char*>(input + processed + 64), _MM_HINT_T0);
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(input + processed));
        acc = _mm_xor_si128(acc, chunk); // XOR acumulativo
    }

    // Procesar bytes restantes con buffer alineado
    if (processed < len) {
        alignas(16) uint8_t buffer[VECTOR_SIZE] = {0};
        std::memcpy(buffer, input + processed, len - processed);
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(buffer));
        acc = _mm_xor_si128(acc, chunk);
    }

    // Reducción final optimizada
    acc = _mm_xor_si128(acc, _mm_shuffle_epi32(acc, _MM_SHUFFLE(1, 0, 3, 2)));
    acc = _mm_xor_si128(acc, _mm_srli_si128(acc, 8));
    acc = _mm_xor_si128(acc, _mm_srli_si128(acc, 4));

    _mm_storeu_si128(reinterpret_cast<__m128i*>(output), acc);
}

bool is_supported() {
    int info[4];
    __cpuidex(info, 1, 0); // Usar __cpuidex para compatibilidad con MSVC
    return (info[3] & (1 << 26)) != 0; // Bit 26 de EDX (SSE2)
}

} // namespace sse2
} // namespace miner