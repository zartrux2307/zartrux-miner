#include "kernel_x86_generic.h"
#include <array>
#include <cstring>
#include <iostream>
#include <iomanip>

namespace miner {
namespace generic {

namespace {
    constexpr size_t HASH_SIZE = 32;

    constexpr uint8_t rotate_left(uint8_t value, uint8_t bits) {
        return (value << bits) | (value >> (8 - bits));
    }
}

void compute_hash(const uint8_t* input, uint8_t* output, size_t len) {
    if (!input || !output || len == 0) {
        std::cerr << "[compute_hash] Entrada inválida, hash no calculado.\n";
        return;
    }

    std::array<uint8_t, HASH_SIZE> hash = {0};

    // XOR circular
    for (size_t i = 0; i < len; ++i) {
        hash[i % HASH_SIZE] ^= input[i];
    }

    // Mezcla de bits
    for (size_t i = 0; i < HASH_SIZE; ++i) {
        hash[i] = rotate_left(hash[i], 1);
    }

    std::memcpy(output, hash.data(), HASH_SIZE);
}

uint64_t run_kernel() {
    constexpr std::array<uint8_t, 64> test_input = {0};
    std::array<uint8_t, HASH_SIZE> output;

    compute_hash(test_input.data(), output.data(), test_input.size());

    std::cout << "[run_kernel] Hash calculado: ";
    for (size_t i = 0; i < HASH_SIZE; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(output[i]);
    }
    std::cout << "\n";

    return 1;
}

} // namespace generic
} // namespace miner