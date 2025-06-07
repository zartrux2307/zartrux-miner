

#include "crypto/randomx/soft_aes.h"

alignas(64) uint32_t lutEnc0[256];
alignas(64) uint32_t lutEnc1[256];
alignas(64) uint32_t lutEnc2[256];
alignas(64) uint32_t lutEnc3[256];

alignas(64) uint32_t lutDec0[256];
alignas(64) uint32_t lutDec1[256];
alignas(64) uint32_t lutDec2[256];
alignas(64) uint32_t lutDec3[256];

static uint32_t mul_gf2(uint32_t b, uint32_t c)
{
	uint32_t s = 0;
	for (uint32_t i = b, j = c, k = 1; (k < 0x100) && j; k <<= 1)
	{
		if (j & k)
		{
			s ^= i;
			j ^= k;
		}

		i <<= 1;
		if (i & 0x100)
			i ^= (1 << 8) | (1 << 4) | (1 << 3) | (1 << 1) | (1 << 0);
	}

	return s;
}

#define ROTL8(x,shift) ((uint8_t) ((x) << (shift)) | ((x) >> (8 - (shift))))

static struct SAESInitializer
{
	SAESInitializer()
	{
		static uint8_t sbox[256];
		static uint8_t sbox_reverse[256];

		uint8_t p = 1, q = 1;

		do {
			p = p ^ (p << 1) ^ (p & 0x80 ? 0x1B : 0);

			q ^= q << 1;
			q ^= q << 2;
			q ^= q << 4;
			q ^= (q & 0x80) ? 0x09 : 0;

			const uint8_t value = q ^ ROTL8(q, 1) ^ ROTL8(q, 2) ^ ROTL8(q, 3) ^ ROTL8(q, 4) ^ 0x63;
			sbox[p] = value;
			sbox_reverse[value] = p;
		} while (p != 1);

		sbox[0] = 0x63;
		sbox_reverse[0x63] = 0;

		for (uint32_t i = 0; i < 0x100; ++i)
		{
			union
			{
				uint32_t w;
				uint8_t p[4];
			};

			uint32_t s = sbox[i];
			p[0] = mul_gf2(s, 2);
			p[1] = s;
			p[2] = s;
			p[3] = mul_gf2(s, 3);

			lutEnc0[i] = w; w = (w << 8) | (w >> 24);
			lutEnc1[i] = w; w = (w << 8) | (w >> 24);
			lutEnc2[i] = w; w = (w << 8) | (w >> 24);
			lutEnc3[i] = w;

			s = sbox_reverse[i];
			p[0] = mul_gf2(s, 0xe);
			p[1] = mul_gf2(s, 0x9);
			p[2] = mul_gf2(s, 0xd);
			p[3] = mul_gf2(s, 0xb);

			lutDec0[i] = w; w = (w << 8) | (w >> 24);
			lutDec1[i] = w; w = (w << 8) | (w >> 24);
			lutDec2[i] = w; w = (w << 8) | (w >> 24);
			lutDec3[i] = w;
		}
	}
} aes_initializer;
