

#pragma once

#include <stdint.h>
#include "crypto/randomx/intrin_portable.h"

extern uint32_t lutEnc0[256];
extern uint32_t lutEnc1[256];
extern uint32_t lutEnc2[256];
extern uint32_t lutEnc3[256];
extern uint32_t lutDec0[256];
extern uint32_t lutDec1[256];
extern uint32_t lutDec2[256];
extern uint32_t lutDec3[256];

template<int soft> rx_vec_i128 aesenc(rx_vec_i128 in, rx_vec_i128 key);
template<int soft> rx_vec_i128 aesdec(rx_vec_i128 in, rx_vec_i128 key);

template<>
FORCE_INLINE rx_vec_i128 aesenc<1>(rx_vec_i128 in, rx_vec_i128 key) {
	volatile uint8_t s[16];
	memcpy((void*) s, &in, 16);

	uint32_t s0 = lutEnc0[s[ 0]];
	uint32_t s1 = lutEnc0[s[ 4]];
	uint32_t s2 = lutEnc0[s[ 8]];
	uint32_t s3 = lutEnc0[s[12]];

	s0 ^= lutEnc1[s[ 5]];
	s1 ^= lutEnc1[s[ 9]];
	s2 ^= lutEnc1[s[13]];
	s3 ^= lutEnc1[s[ 1]];

	s0 ^= lutEnc2[s[10]];
	s1 ^= lutEnc2[s[14]];
	s2 ^= lutEnc2[s[ 2]];
	s3 ^= lutEnc2[s[ 6]];

	s0 ^= lutEnc3[s[15]];
	s1 ^= lutEnc3[s[ 3]];
	s2 ^= lutEnc3[s[ 7]];
	s3 ^= lutEnc3[s[11]];

	return rx_xor_vec_i128(rx_set_int_vec_i128(s3, s2, s1, s0), key);
}

template<>
FORCE_INLINE rx_vec_i128 aesdec<1>(rx_vec_i128 in, rx_vec_i128 key) {
	volatile uint8_t s[16];
	memcpy((void*) s, &in, 16);

	uint32_t s0 = lutDec0[s[ 0]];
	uint32_t s1 = lutDec0[s[ 4]];
	uint32_t s2 = lutDec0[s[ 8]];
	uint32_t s3 = lutDec0[s[12]];

	s0 ^= lutDec1[s[13]];
	s1 ^= lutDec1[s[ 1]];
	s2 ^= lutDec1[s[ 5]];
	s3 ^= lutDec1[s[ 9]];

	s0 ^= lutDec2[s[10]];
	s1 ^= lutDec2[s[14]];
	s2 ^= lutDec2[s[ 2]];
	s3 ^= lutDec2[s[ 6]];

	s0 ^= lutDec3[s[ 7]];
	s1 ^= lutDec3[s[11]];
	s2 ^= lutDec3[s[15]];
	s3 ^= lutDec3[s[ 3]];

	return rx_xor_vec_i128(rx_set_int_vec_i128(s3, s2, s1, s0), key);
}

template<>
FORCE_INLINE rx_vec_i128 aesenc<2>(rx_vec_i128 in, rx_vec_i128 key) {
	uint32_t s0, s1, s2, s3;

	s0 = rx_vec_i128_w(in);
	s1 = rx_vec_i128_z(in);
	s2 = rx_vec_i128_y(in);
	s3 = rx_vec_i128_x(in);

	rx_vec_i128 out = rx_set_int_vec_i128(
		(lutEnc0[s0 & 0xff] ^ lutEnc1[(s3 >> 8) & 0xff] ^ lutEnc2[(s2 >> 16) & 0xff] ^ lutEnc3[s1 >> 24]),
		(lutEnc0[s1 & 0xff] ^ lutEnc1[(s0 >> 8) & 0xff] ^ lutEnc2[(s3 >> 16) & 0xff] ^ lutEnc3[s2 >> 24]),
		(lutEnc0[s2 & 0xff] ^ lutEnc1[(s1 >> 8) & 0xff] ^ lutEnc2[(s0 >> 16) & 0xff] ^ lutEnc3[s3 >> 24]),
		(lutEnc0[s3 & 0xff] ^ lutEnc1[(s2 >> 8) & 0xff] ^ lutEnc2[(s1 >> 16) & 0xff] ^ lutEnc3[s0 >> 24])
	);

	return rx_xor_vec_i128(out, key);
}

template<>
FORCE_INLINE rx_vec_i128 aesdec<2>(rx_vec_i128 in, rx_vec_i128 key) {
	uint32_t s0, s1, s2, s3;

	s0 = rx_vec_i128_w(in);
	s1 = rx_vec_i128_z(in);
	s2 = rx_vec_i128_y(in);
	s3 = rx_vec_i128_x(in);

	rx_vec_i128 out = rx_set_int_vec_i128(
		(lutDec0[s0 & 0xff] ^ lutDec1[(s1 >> 8) & 0xff] ^ lutDec2[(s2 >> 16) & 0xff] ^ lutDec3[s3 >> 24]),
		(lutDec0[s1 & 0xff] ^ lutDec1[(s2 >> 8) & 0xff] ^ lutDec2[(s3 >> 16) & 0xff] ^ lutDec3[s0 >> 24]),
		(lutDec0[s2 & 0xff] ^ lutDec1[(s3 >> 8) & 0xff] ^ lutDec2[(s0 >> 16) & 0xff] ^ lutDec3[s1 >> 24]),
		(lutDec0[s3 & 0xff] ^ lutDec1[(s0 >> 8) & 0xff] ^ lutDec2[(s1 >> 16) & 0xff] ^ lutDec3[s2 >> 24])
	);

	return rx_xor_vec_i128(out, key);
}

template<>
FORCE_INLINE rx_vec_i128 aesenc<0>(rx_vec_i128 in, rx_vec_i128 key) {
	return rx_aesenc_vec_i128(in, key);
}

template<>
FORCE_INLINE rx_vec_i128 aesdec<0>(rx_vec_i128 in, rx_vec_i128 key) {
	return rx_aesdec_vec_i128(in, key);
}
