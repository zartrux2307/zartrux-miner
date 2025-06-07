

#ifndef PORTABLE_BLAKE2_IMPL_H
#define PORTABLE_BLAKE2_IMPL_H

#include <stdint.h>

#include "crypto/randomx/blake2/endian.h"

static FORCE_INLINE uint64_t load48(const void *src) {
	const uint8_t *p = (const uint8_t *)src;
	uint64_t w = *p++;
	w |= (uint64_t)(*p++) << 8;
	w |= (uint64_t)(*p++) << 16;
	w |= (uint64_t)(*p++) << 24;
	w |= (uint64_t)(*p++) << 32;
	w |= (uint64_t)(*p++) << 40;
	return w;
}

static FORCE_INLINE void store48(void *dst, uint64_t w) {
	uint8_t *p = (uint8_t *)dst;
	*p++ = (uint8_t)w;
	w >>= 8;
	*p++ = (uint8_t)w;
	w >>= 8;
	*p++ = (uint8_t)w;
	w >>= 8;
	*p++ = (uint8_t)w;
	w >>= 8;
	*p++ = (uint8_t)w;
	w >>= 8;
	*p++ = (uint8_t)w;
}

static FORCE_INLINE uint32_t rotr32(const uint32_t w, const unsigned c) {
	return (w >> c) | (w << (32 - c));
}

static FORCE_INLINE uint64_t rotr64(const uint64_t w, const unsigned c) {
	return (w >> c) | (w << (64 - c));
}

#endif
