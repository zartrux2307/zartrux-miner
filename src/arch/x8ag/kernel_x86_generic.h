#pragma once

#include <cstdint>
#include <cstddef>

namespace miner {
namespace generic {

// Declaraciones limpias sin implementaciones inline
void compute_hash(const uint8_t* input, uint8_t* output, size_t len);
uint64_t run_kernel();

} // namespace generic
} // namespace miner