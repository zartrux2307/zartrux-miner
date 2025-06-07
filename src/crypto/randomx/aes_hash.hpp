

#pragma once

#include <cstddef>

typedef void (hashAndFillAes1Rx4_impl)(void *scratchpad, size_t scratchpadSize, void *hash, void* fill_state);

extern hashAndFillAes1Rx4_impl* softAESImpl;

inline hashAndFillAes1Rx4_impl* GetSoftAESImpl()
{
  return softAESImpl;
}

void SelectSoftAESImpl(size_t threadsCount);

template<int softAes>
void hashAes1Rx4(const void *input, size_t inputSize, void *hash);

template<int softAes>
void fillAes1Rx4(void *state, size_t outputSize, void *buffer);

template<int softAes>
void fillAes4Rx4(void *state, size_t outputSize, void *buffer);

template<int softAes, int unroll>
void hashAndFillAes1Rx4(void *scratchpad, size_t scratchpadSize, void *hash, void* fill_state);
