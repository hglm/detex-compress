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

void SeedRGTC1(const detexBlockInfo * DETEX_RESTRICT info, dstCMWCRNG * DETEX_RESTRICT rng,
uint8_t * DETEX_RESTRICT bitstring) {
	uint32_t red_values = rng->RandomBits(16);
	// Set the mode for RGTC1.
	if (info->mode >= 0) {
		int m = (red_values & 0xFF) <= (red_values >> 8);
		if (m != info->mode)
			red_values = ((red_values & 0xFF) << 8) | (red_values >> 8);
	}
	*(uint16_t *)(bitstring) = red_values;
}

static const uint32_t detex_bc4_component_mask[6] = {
	0xFF, 0xFF00
};

static const uint8_t detex_bc4_component_shift[6] = {
	0, 8
};

static const int8_t detex_bc4_mutation_table1[8][3] = {
	{ 0, -1, 0 },	// red0
	{ 0, -1, 0 },	// red0
	{ 0, -1, 0 },	// red0
	{ 1, -1, 0 },	// red1
	{ 1, -1, 0 },	// red1
	{ 1, -1, 0 },	// red1
	{ 0, 1, -1 },	// red0, red1
	{ 0, 1, -1 },	// red0, red1
};

static const uint8_t detex_bc4_offset_random_bits_table[] = {
	6, 6, 6, 6, 6, 6, 6, 6,	// Random 1 to 64, generation 128-1023 (unused)
	6,			// Random 1 to 64, generation 1024-1151
	5,			// Random 1 to 32, generation 1152-1280
	4,			// Random 1 to 16, generation 1280-1407
	3,			// Random 1 to 8, generation 1408-1535
	2, 2,			// Random 1 to 4, generation 1536 to 1791
	1, 1			// Random 1 to 2, generation 1792 to 2047
};

void MutateRGTC1(const detexBlockInfo * DETEX_RESTRICT info, dstCMWCRNG * DETEX_RESTRICT rng, int generation,
uint8_t * DETEX_RESTRICT bitstring) {
	// Mutate red base values.
	uint16_t *bitstring16 = (uint16_t *)bitstring;
	uint32_t red_values = *bitstring16;
	if (generation < 1024) {
		// For generations < 1024, replace components entirely with a random
		// value.
		for (;;) {
			int mutation_type = rng->RandomBits(3);
			const int8_t *mutationp = detex_bc4_mutation_table1[mutation_type];
			for (;*mutationp >= 0; mutationp++) {
				int component = *mutationp;
				uint32_t mask = detex_bc4_component_mask[component];
				// Set the component to a random value.
				int value = rng->RandomBits(8);
				red_values &= ~mask;
				red_values |= value << detex_bc4_component_shift[component];
			}
			int m = (red_values & 0xFF) <= (red_values >> 8);
			if (info->mode < 0 || m == info->mode)
				break;
		}
		*(uint16_t *)bitstring16 = red_values;
		return;
	}
	// For generations >= 1024, apply diminishing random offset to components.
	int generation_table_index = generation / 128;
	if (generation_table_index > 15)
		generation_table_index = 15;
	for (;;) {
		int mutation_type = rng->RandomBits(3);
		const int8_t *mutationp = detex_bc4_mutation_table1[mutation_type];
		for (;*mutationp >= 0; mutationp++) {
			int component = *mutationp;
			uint32_t mask = detex_bc4_component_mask[component];
			int value = (red_values & mask) >> detex_bc4_component_shift[component];
			int offset;
			int sign_bit;
			int offset_random_bits = detex_bc4_offset_random_bits_table[generation_table_index];
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
			red_values &= ~mask;
			red_values |= value << detex_bc4_component_shift[component];
		}
		int m = (red_values & 0xFF) <= (red_values >> 8);
		if (info->mode < 0 || m == info->mode)
			break;
	}
	*(uint16_t *)bitstring16 = red_values;
}

static DETEX_INLINE_ONLY uint32_t SetPixelXYRGTC1(const uint8_t * DETEX_RESTRICT pix_orig, int stride_orig,
int dx, int dy, const int * DETEX_RESTRICT red, uint64_t & DETEX_RESTRICT red_pixel_indices) {
	int red_orig = *(uint8_t *)(pix_orig + dy * stride_orig + dx);
	uint32_t best_error = (red_orig - red[0]) * (red_orig - red[0]);
	int best_pixel_index = 0;
	for (int i = 1; i < 8; i++) {
		uint32_t error = (red_orig - red[i]) * (red_orig - red[i]);
		if (error < best_error) {
			best_error = error;
			best_pixel_index = i;
		}
	}
	int i = dy * 4 + dx;
	red_pixel_indices |= (uint64_t)best_pixel_index << (i * 3);
	return best_error;
}

static DETEX_INLINE_ONLY void DecodeRedRGTC1(int *red) {
	if (red[0] > red[1]) {
		red[2] = detexDivide0To1791By7(6 * red[0] + 1 * red[1]);
		red[3] = detexDivide0To1791By7(5 * red[0] + 2 * red[1]);
		red[4] = detexDivide0To1791By7(4 * red[0] + 3 * red[1]);
		red[5] = detexDivide0To1791By7(3 * red[0] + 4 * red[1]);
		red[6] = detexDivide0To1791By7(2 * red[0] + 5 * red[1]);
		red[7] = detexDivide0To1791By7(1 * red[0] + 6 * red[1]);
	}
	else {
		red[2] = detexDivide0To1279By5(4 * red[0] + 1 * red[1]);
		red[3] = detexDivide0To1279By5(3 * red[0] + 2 * red[1]);
		red[4] = detexDivide0To1279By5(2 * red[0] + 3 * red[1]);
		red[5] = detexDivide0To1279By5(1 * red[0] + 4 * red[1]);
		red[6] = 0;
		red[7] = 0xFF;
	}
}

uint32_t SetPixelsRGTC1(const detexBlockInfo * DETEX_RESTRICT info, uint8_t * DETEX_RESTRICT bitstring) {
	const detexTexture *texture = info->texture;
	int x = info->x;
	int y = info->y;
	int red[8];
	red[0] = bitstring[0];
	red[1] = bitstring[1];
	DecodeRedRGTC1(red);
	uint8_t *pix_orig = texture->data + y * texture->width + x;
	int stride_orig = texture->width;
	uint64_t red_pixel_indices = 0;
	uint32_t error = SetPixelXYRGTC1(pix_orig, stride_orig, 0, 0, red, red_pixel_indices);
	error += SetPixelXYRGTC1(pix_orig, stride_orig, 1, 0, red, red_pixel_indices);
	error += SetPixelXYRGTC1(pix_orig, stride_orig, 2, 0, red, red_pixel_indices);
	error += SetPixelXYRGTC1(pix_orig, stride_orig, 3, 0, red, red_pixel_indices);
	error += SetPixelXYRGTC1(pix_orig, stride_orig, 0, 1, red, red_pixel_indices);
	error += SetPixelXYRGTC1(pix_orig, stride_orig, 1, 1, red, red_pixel_indices);
	error += SetPixelXYRGTC1(pix_orig, stride_orig, 2, 1, red, red_pixel_indices);
	error += SetPixelXYRGTC1(pix_orig, stride_orig, 3, 1, red, red_pixel_indices);
	error += SetPixelXYRGTC1(pix_orig, stride_orig, 0, 2, red, red_pixel_indices);
	error += SetPixelXYRGTC1(pix_orig, stride_orig, 1, 2, red, red_pixel_indices);
	error += SetPixelXYRGTC1(pix_orig, stride_orig, 2, 2, red, red_pixel_indices);
	error += SetPixelXYRGTC1(pix_orig, stride_orig, 3, 2, red, red_pixel_indices);
	error += SetPixelXYRGTC1(pix_orig, stride_orig, 0, 3, red, red_pixel_indices);
	error += SetPixelXYRGTC1(pix_orig, stride_orig, 1, 3, red, red_pixel_indices);
	error += SetPixelXYRGTC1(pix_orig, stride_orig, 2, 3, red, red_pixel_indices);
	error += SetPixelXYRGTC1(pix_orig, stride_orig, 3, 3, red, red_pixel_indices);
	*(uint64_t *)bitstring = *(uint16_t *)bitstring | (red_pixel_indices << 16);
	return error;
}

