/*

Copyright (c) 2015 Harm Hanemaaijer <fgenfb@yahoo.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/

#include <dstRandom.h>
#include "detex.h"
#include "compress-block.h"

static void SetAlphaPixelsBC2(const detexTexture * DETEX_RESTRICT texture, int x, int y,
uint8_t * DETEX_RESTRICT bitstring) {
	uint8_t *pix_orig = texture->data + (y * texture->width + x) * 4;
	int stride_orig = texture->width * 4;
	uint64_t alpha_pixels = 0;
	for (y = 0; y < 4;  y++)
		for (int x = 0; x < 4; x++) {
			uint32_t alpha_value = pix_orig[y * stride_orig + x * 4 + 3];
			// Scale and round the alpha value to the values allowed by BC2.
			uint32_t alpha_pixel = (alpha_value * 15 + 8) / 255;
			int i = y * 4 + x;
			alpha_pixels |= (uint64_t)alpha_pixel << (i * 4);
		}
	*(uint64_t *)&bitstring[0] = alpha_pixels;
}

void SeedBC2(const detexBlockInfo * DETEX_RESTRICT info, dstCMWCRNG * DETEX_RESTRICT rng,
uint8_t * DETEX_RESTRICT bitstring) {
	// Only seed the color values.
	uint32_t color_values = rng->Random32();
	uint32_t *color_values_p32 = (uint32_t *)(bitstring + 8);
	*(uint32_t *)color_values_p32 = color_values;
	// Ensure the colors correspond to mode 0 of BC1.
	detexSetModeBC1(bitstring + 8, 0, 0, NULL);
	SetAlphaPixelsBC2(info->texture, info->x, info->y, bitstring);
}

void MutateBC2(const detexBlockInfo * DETEX_RESTRICT info, dstCMWCRNG * DETEX_RESTRICT rng, int generation,
uint8_t * DETEX_RESTRICT bitstring) {
	// Since mode is always 0 for BC2, the correct BC1 color mode (0) will be passed.
	MutateBC1(info, rng, generation, bitstring + 8);
}

uint32_t SetPixelsBC2(const detexBlockInfo * DETEX_RESTRICT info, uint8_t * DETEX_RESTRICT bitstring) {
	return SetPixelsBC1(info, bitstring + 8);
}

void SeedBC3(const detexBlockInfo * DETEX_RESTRICT info, dstCMWCRNG * DETEX_RESTRICT rng,
uint8_t * DETEX_RESTRICT bitstring) {
	// Only seed the color and alpha values.
	uint32_t color_values = rng->Random32();
	uint32_t *color_values_p32 = (uint32_t *)(bitstring + 8);
	*(uint32_t *)color_values_p32 = color_values;
	uint32_t alpha_values = rng->RandomBits(16);
	// Ensure the colors correspond to mode 0 of BC1.
	detexSetModeBC1(bitstring + 8, 0, 0, NULL);
	// Set the mode for BC3.
	if (info->mode >= 0) {
		int m = (alpha_values & 0xFF) <= (alpha_values >> 8);
		if (m != info->mode)
			alpha_values = ((alpha_values & 0xFF) << 8) | (alpha_values >> 8);
	}
	*(uint16_t *)(bitstring) = alpha_values;
}

static const uint32_t detex_bc3_component_mask[6] = {
	0xFF, 0xFF00
};

static const uint8_t detex_bc3_component_shift[6] = {
	0, 8
};

static const int8_t detex_bc3_mutation_table1[8][3] = {
	{ 0, -1, 0 },	// alpha0
	{ 0, -1, 0 },	// alpha0
	{ 0, -1, 0 },	// alpha0
	{ 1, -1, 0 },	// alpha1
	{ 1, -1, 0 },	// alpha1
	{ 1, -1, 0 },	// alpha1
	{ 0, 1, -1 },	// alpha0, alpha1
	{ 0, 1, -1 },	// alpha0, alpha1
};

static const uint8_t detex_bc3_offset_random_bits_table[] = {
	6, 6, 6, 6, 6, 6, 6, 6,	// Random 1 to 64, generation 128-1023 (unused)
	6,			// Random 1 to 64, generation 1024-1151
	5,			// Random 1 to 32, generation 1152-1280
	4,			// Random 1 to 16, generation 1280-1407
	3,			// Random 1 to 8, generation 1408-1535
	2, 2,			// Random 1 to 4, generation 1536 to 1791
	1, 1			// Random 1 to 2, generation 1792 to 2047
};

void MutateBC3(const detexBlockInfo * DETEX_RESTRICT info, dstCMWCRNG * DETEX_RESTRICT rng, int generation,
uint8_t * DETEX_RESTRICT bitstring) {
	detexBlockInfo info2 = *info;
	info2.mode = 0;
	MutateBC1(&info2, rng, generation, bitstring + 8);
	// Mutate alpha base values.
	uint16_t *bitstring16 = (uint16_t *)bitstring;
	uint32_t alpha_values = *bitstring16;
	if (generation < 1024) {
		// For generations < 1024, replace components entirely with a random
		// value.
		for (;;) {
			int mutation_type = rng->RandomBits(3);
			const int8_t *mutationp = detex_bc3_mutation_table1[mutation_type];
			for (;*mutationp >= 0; mutationp++) {
				int component = *mutationp;
				uint32_t mask = detex_bc3_component_mask[component];
				// Set the component to a random value.
				int value = rng->RandomBits(8);
				alpha_values &= ~mask;
				alpha_values |= value << detex_bc3_component_shift[component];
			}
			int m = (alpha_values & 0xFF) <= (alpha_values >> 8);
			if (info->mode < 0 || m == info->mode)
				break;
		}
		*(uint16_t *)bitstring16 = alpha_values;
		return;
	}
	// For generations >= 1024, apply diminishing random offset to components.
	int generation_table_index = generation / 128;
	if (generation_table_index > 15)
		generation_table_index = 15;
	for (;;) {
		int mutation_type = rng->RandomBits(3);
		const int8_t *mutationp = detex_bc3_mutation_table1[mutation_type];
		for (;*mutationp >= 0; mutationp++) {
			int component = *mutationp;
			uint32_t mask = detex_bc3_component_mask[component];
			int value = (alpha_values & mask) >> detex_bc3_component_shift[component];
			int offset;
			int sign_bit;
			int offset_random_bits = detex_bc3_offset_random_bits_table[generation_table_index];
			int rnd = rng->RandomBits(offset_random_bits + 1);
			sign_bit = rnd & 1;
			offset = (rnd >> 1) + 1;
			// Apply positive or negative displacement.
			if (sign_bit == 0) {
				value += offset;
				if (value > 0xFF)
					value = 0xFF;
			}
			else {
				value -= offset;
				if (value < 0)
					value = 0;
			}
			alpha_values &= ~mask;
			alpha_values |= value << detex_bc3_component_shift[component];
		}
		int m = (alpha_values & 0xFF) <= (alpha_values >> 8);
		if (info->mode < 0 || m == info->mode)
			break;
	}
	*(uint16_t *)bitstring16 = alpha_values;

}

static DETEX_INLINE_ONLY uint32_t SetAlphaPixelXYBC3(const uint8_t * DETEX_RESTRICT pix_orig, int stride_orig,
int dx, int dy, const int * DETEX_RESTRICT alpha, uint64_t & DETEX_RESTRICT alpha_pixel_indices) {
	uint32_t pixel_orig = *(uint32_t *)(pix_orig + dy * stride_orig + dx * 4);
	int a_orig = detexPixel32GetA8(pixel_orig);
	uint32_t best_error = (a_orig - alpha[0]) * (a_orig - alpha[0]);
	int best_pixel_index = 0;
	for (int i = 1; i < 8; i++) {
		uint32_t error = (a_orig - alpha[i]) * (a_orig - alpha[i]);
		if (error < best_error) {
			best_error = error;
			best_pixel_index = i;
		}
	}
	int i = dy * 4 + dx;
	alpha_pixel_indices |= (uint64_t)best_pixel_index << (i * 3);
	return best_error;
}

static DETEX_INLINE_ONLY void DecodeAlphaBC3(int *alpha) {
	if (alpha[0] > alpha[1]) {
		alpha[2] = detexDivide0To1791By7(6 * alpha[0] + 1 * alpha[1]);
		alpha[3] = detexDivide0To1791By7(5 * alpha[0] + 2 * alpha[1]);
		alpha[4] = detexDivide0To1791By7(4 * alpha[0] + 3 * alpha[1]);
		alpha[5] = detexDivide0To1791By7(3 * alpha[0] + 4 * alpha[1]);
		alpha[6] = detexDivide0To1791By7(2 * alpha[0] + 5 * alpha[1]);
		alpha[7] = detexDivide0To1791By7(1 * alpha[0] + 6 * alpha[1]);
	}
	else {
		alpha[2] = detexDivide0To1279By5(4 * alpha[0] + 1 * alpha[1]);
		alpha[3] = detexDivide0To1279By5(3 * alpha[0] + 2 * alpha[1]);
		alpha[4] = detexDivide0To1279By5(2 * alpha[0] + 3 * alpha[1]);
		alpha[5] = detexDivide0To1279By5(1 * alpha[0] + 4 * alpha[1]);
		alpha[6] = 0;
		alpha[7] = 0xFF;
	}
}

uint32_t SetPixelsBC3(const detexBlockInfo * DETEX_RESTRICT info, uint8_t * DETEX_RESTRICT bitstring) {
	const detexTexture *texture = info->texture;
	int x = info->x;
	int y = info->y;
	uint32_t error = SetPixelsBC1(info, bitstring + 8);
	int alpha[8];
	alpha[0] = bitstring[0];
	alpha[1] = bitstring[1];
	DecodeAlphaBC3(alpha);
	uint8_t *pix_orig = texture->data + (y * texture->width + x) * 4;
	int stride_orig = texture->width * 4;
	uint64_t alpha_pixel_indices = 0;
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 0, 0, alpha, alpha_pixel_indices);
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 1, 0, alpha, alpha_pixel_indices);
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 2, 0, alpha, alpha_pixel_indices);
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 3, 0, alpha, alpha_pixel_indices);
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 0, 1, alpha, alpha_pixel_indices);
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 1, 1, alpha, alpha_pixel_indices);
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 2, 1, alpha, alpha_pixel_indices);
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 3, 1, alpha, alpha_pixel_indices);
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 0, 2, alpha, alpha_pixel_indices);
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 1, 2, alpha, alpha_pixel_indices);
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 2, 2, alpha, alpha_pixel_indices);
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 3, 2, alpha, alpha_pixel_indices);
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 0, 3, alpha, alpha_pixel_indices);
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 1, 3, alpha, alpha_pixel_indices);
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 2, 3, alpha, alpha_pixel_indices);
	error += SetAlphaPixelXYBC3(pix_orig, stride_orig, 3, 3, alpha, alpha_pixel_indices);
	*(uint64_t *)bitstring = *(uint16_t *)bitstring | (alpha_pixel_indices << 16);
	// Recalculate error because alpha values influence color error result (when both alpha
	// values are zero, the color values are disregarded).
	uint8_t pixel_buffer[64];
	detexDecompressBlockBC3(bitstring, 0x3, 0, pixel_buffer);
	error = detexCalculateErrorRGBA8(texture, x, y, pixel_buffer);
	return error;
}

