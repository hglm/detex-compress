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
#ifdef __SSE2__
#define DST_SIMD_MODE_SSE2
#include <dstSIMD.h>
#endif
#include "detex.h"
#include "compress-block.h"

// Components: red1, green1, blue1, red2, green2, blue2, code_word1, code_word2
static const uint32_t detex_etc1_individual_component_mask[8] = {
	0x000000F0, 0x0000F000, 0x00F00000, 0x0000000F, 0x00000F00, 0x000F0000, 0xE0000000, 0x1C000000
};

static const uint8_t detex_etc1_individual_component_shift[8] = {
	4, 12, 20, 0, 8, 16, 29, 26
};

static const int detex_etc1_individual_component_max_value[8] = {
	15, 15, 15, 15, 15, 15, 7, 7
};

static const uint32_t detex_etc1_differential_component_mask[8] = {
	0x000000F8, 0x0000F800, 0x00F80000, 0x00000007, 0x00000700, 0x00070000, 0xE0000000, 0x1C000000
};

static const uint8_t detex_etc1_differential_component_shift[8] = {
	3, 11, 19, 0, 8, 16, 29, 26
};

static const int detex_etc1_differential_component_max_value[8] = {
	31, 31, 31, 7, 7, 7, 7, 7
};

static const int8_t detex_etc1_mutation_table1[16][8] = {
	{ 0, 3, -1, 0, 0, 0, 0, 0 },	// red1, red2
	{ 0, 3, -1, 0, 0, 0, 0, 0 },	// red1, red2
	{ 0, 3, -1, 0, 0, 0, 0, 0 },	// red1, red2
	{ 1, 4, -1, 0, 0, 0, 0, 0 },	// green1, green2
	{ 1, 4, -1, 0, 0, 0, 0, 0 },	// green1, green2
	{ 1, 4, -1, 0, 0, 0, 0, 0 },	// green1, green2
	{ 2, 5, -1, 0, 0, 0, 0, 0 },	// blue1, blue2
	{ 2, 5, -1, 0, 0, 0, 0, 0 },	// blue1, blue2
	{ 2, 5, -1, 0, 0, 0, 0, 0 },	// blue1, blue2
	{ 0, 1, 2, -1, 0, 0, 0, 0 },	// red1, green1, blue1
	{ 0, 1, 2, -1, 0, 0, 0, 0 },	// red1, green1, blue1
	{ 3, 4, 5, -1, 0, 0, 0, 0 },	// red2, green2, blue2
	{ 3, 4, 5, -1, 0, 0, 0, 0 },	// red2, green2, blue2
	{ 0, 1, 2, 3, 4, 5, -1, 0 },	// red1, green1, blue1, red2, green2, blue2
	{ 6, 7, 0, 0, 0, 0, 0, 0 },	// codeword1, codeword2
	{ 6, 7, 0, 0, 0, 0, 0, 0 },	// codeword1, codeword2
};

static const int8_t detex_etc1_mutation_table2[16][8] = {
	{ 0, -1, 0, 0, 0, 0, 0, 0 },	// red1
	{ 1, -1, 0, 0, 0, 0, 0, 0 },	// green1
	{ 2, -1, 0, 0, 0, 0, 0, 0 },	// blue1
	{ 3, -1, 0, 0, 0, 0, 0, 0 },	// red2
	{ 4, -1, 0, 0, 0, 0, 0, 0 },	// green2
	{ 5, -1, 0, 0, 0, 0, 0, 0 },	// blue2
	{ 6, -1, 0, 0, 0, 0, 0, 0 },	// codeword1
	{ 7, -1, 0, 0, 0, 0, 0, 0 },	// codeword2
	{ 0, 3, -1, 0, 0, 0, 0, 0 },	// red1, red2
	{ 0, 3, -1, 0, 0, 0, 0, 0 },	// red1, red2
	{ 1, 4, -1, 0, 0, 0, 0, 0 },	// green1, green2
	{ 1, 4, -1, 0, 0, 0, 0, 0 },	// green1, green2
	{ 2, 5, -1, 0, 0, 0, 0, 0 },	// blue1, blue2
	{ 2, 5, -1, 0, 0, 0, 0, 0 },	// blue1, blue2
	{ 0, 1, 2, -1, 0, 0, 0, 0 },	// red1, green1, blue1
	{ 3, 4, 5, -1, 0, 0, 0, 0 },	// red2, green2, blue2
};

static const uint8_t detex_etc1_individual_offset_random_bits_table[] = {
	3, 3, 3, 3, 3, 3, 3, 3,	// Random 1 to 8, generation 128-1023 (unused)
	3,			// Random 1 to 8, generation 1024-1151
	3,			// Random 1 to 8, generation 1152-1279
	2, 2,			// Random 1 to 4, generation 1280-1535
	1, 1, 1, 1,		// Random 1 to 2, generation 1536-2043
};

static const uint8_t detex_etc1_differential_color1_offset_random_bits_table[] = {
	4, 4, 4, 4, 4, 4, 4, 4,	// Random 1 to 16, generation 128-1023 (unused)
	4,			// Random 1 to 16, generation 1024-1151
	3,			// Random 1 to 8, generation 1152-1279
	2, 2,			// Random 1 to 4, generation 1280-1535
	1, 1, 1, 1,		// Random 1 to 2, generation 1536-2043
};

static const uint8_t detex_etc1_differential_color2_offset_random_bits_table[] = {
	2, 2, 2, 2, 2, 2, 2, 2,	// Random 1 to 4, generation 128-1023 (unused)
	2,			// Random 1 to 4, generation 1024-1151
	2,			// Random 1 to 4, generation 1152-1279
	2, 2,			// Random 1 to 4, generation 1280-1535
	1, 1, 1, 1,		// Random 1 to 2, generation 1536-2043
};

static const uint8_t detex_etc1_codeword_offset_random_bits_table[] = {
	2, 2, 2, 2, 2, 2, 2, 2,	// Random 1 to 4, generation 128-1023 (unused)
	2,			// Random 1 to 4, generation 1024-1151
	2,			// Random 1 to 4, generation 1152-1279
	1, 1,			// Random 1 to 2, generation 1280-1535
	1, 1, 1, 1,		// Random 1 to 2, generation 1536-2043
};

static const int complement3bitshifted_table[8] = {
	0, 8, 16, 24, -32, -24, -16, -8
};

// This function calculates the 3-bit complement value in the range -4 to 3 of a three bit
// representation. The result is arithmetically shifted 3 places to the left before returning.
static DETEX_INLINE_ONLY int complement3bitshifted(int x) {
	return complement3bitshifted_table[x];
}

static DETEX_INLINE_ONLY bool ColorsAreInvalidETC1Differential(uint32_t bits) {
	// Check for overflow with differential mode.
	int R = bits & 0xF8;
	R += complement3bitshifted(bits & 0x7);
	int G = (bits >> 8) & 0xF8;
	G += complement3bitshifted((bits >> 8) & 0x7);
	int B = (bits >> 16) & 0xF8;
	B += complement3bitshifted((bits >> 16) & 0x7);
	if ((R & 0xFF07) || (G & 0xFF07) || (B & 0xFF07))
		return true;
	else
		return false;
}

void SeedETC1(const detexBlockInfo * DETEX_RESTRICT info, dstCMWCRNG *rng, uint8_t * DETEX_RESTRICT bitstring) {
	// Only need to initialize the color/mode fields. The pixel values will be set later.
	uint32_t bits;
	for (;;) {
		bits = rng->Random32();
		bits = (bits & (~0x03000000)) | (info->mode << 24);
		if (info->mode & 2) {
			if (ColorsAreInvalidETC1Differential(bits))
				continue;
		}
		break;
	}
	uint32_t *bitstring32 = (uint32_t *)bitstring;
	*(uint32_t *)bitstring32 = bits;
}

static void MutateETC1Individual(const detexBlockInfo * DETEX_RESTRICT info, dstCMWCRNG * DETEX_RESTRICT rng, int generation,
uint8_t * DETEX_RESTRICT bitstring) {
	uint32_t *bitstring32 = (uint32_t *)bitstring;
	uint32_t colors = *bitstring32;
	if (generation < 1024) {
		// For generations < 1024, replace components entirely with a random
		// value.
		for (;;) {
			int mutation_type = rng->RandomBits(4);
			const int8_t *mutationp = detex_etc1_mutation_table1[mutation_type];
			for (;*mutationp >= 0; mutationp++) {
				int component = *mutationp;
				uint32_t mask = detex_etc1_individual_component_mask[component];
				// Set the component to a random value.
				int value = rng->RandomBits(4) & detex_etc1_individual_component_max_value[component];
				colors &= ~mask;
				colors |= value << detex_etc1_individual_component_shift[component];
			}
			// Note: Non-modal operation not supported.
			break;
		}
		*(uint32_t *)bitstring32 = colors;
		return;
	}
	// For generations >= 1024, apply diminishing random offset to components.
	int generation_table_index = generation / 128;
	if (generation_table_index > 15)
		generation_table_index = 15;
	for (;;) {
		int mutation_type = rng->RandomBits(4);
		const int8_t *mutationp = detex_etc1_mutation_table2[mutation_type];
		for (;*mutationp >= 0; mutationp++) {
			int component = *mutationp;
			uint32_t mask = detex_etc1_individual_component_mask[component];
			int value = (colors & mask) >> detex_etc1_individual_component_shift[component];
			int offset;
			int sign_bit;
			int offset_random_bits;
			if (component < 6)
				offset_random_bits =
					detex_etc1_individual_offset_random_bits_table[generation_table_index];
			else
				offset_random_bits =
					detex_etc1_codeword_offset_random_bits_table[generation_table_index];
			int rnd = rng->RandomBits(offset_random_bits + 1);
			sign_bit = rnd & 1;
			offset = (rnd >> 1) + 1;
			// Apply positive or negative displacement.
			if (sign_bit == 0) {
				value += offset;
				if (value > detex_etc1_individual_component_max_value[component])
					value = detex_etc1_individual_component_max_value[component];
			}
			else {
				value -= offset;
				if (value < 0)
					value = 0;
			}
			colors &= ~mask;
			colors |= value << detex_etc1_individual_component_shift[component];
		}
		break;
	}
	*(uint32_t *)bitstring32 = colors;
}

static void MutateETC1Differential(const detexBlockInfo * DETEX_RESTRICT info, dstCMWCRNG * DETEX_RESTRICT rng, int generation,
uint8_t * DETEX_RESTRICT bitstring) {
	uint32_t *bitstring32 = (uint32_t *)bitstring;
	uint32_t colors = *bitstring32;
	if (generation < 1024) {
		// For generations < 1024, replace components entirely with a random
		// value.
		for (;;) {
			int mutation_type = rng->RandomBits(4);
			const int8_t *mutationp = detex_etc1_mutation_table1[mutation_type];
			for (;*mutationp >= 0; mutationp++) {
				int component = *mutationp;
				uint32_t mask = detex_etc1_differential_component_mask[component];
				// Set the component to a random value.
				int value = rng->RandomBits(5) & detex_etc1_differential_component_max_value[component];
				colors &= ~mask;
				colors |= value << detex_etc1_differential_component_shift[component];
			}
			if (!ColorsAreInvalidETC1Differential(colors))
				break;
			// Note: Non-modal operation not supported.
		}
		*(uint32_t *)bitstring32 = colors;
		return;
	}
	// For generations >= 1024, apply diminishing random offset to components.
	int generation_table_index = generation / 128;
	if (generation_table_index > 15)
		generation_table_index = 15;
	for (;;) {
		int mutation_type = rng->RandomBits(4);
		const int8_t *mutationp = detex_etc1_mutation_table2[mutation_type];
		for (;*mutationp >= 0; mutationp++) {
			int component = *mutationp;
			uint32_t mask = detex_etc1_differential_component_mask[component];
			int value = (colors & mask) >> detex_etc1_differential_component_shift[component];
			int offset;
			int sign_bit;
			int offset_random_bits;
			if (component < 3)
				offset_random_bits =
					detex_etc1_differential_color1_offset_random_bits_table[generation_table_index];
			else if (component < 6)
				offset_random_bits =
					detex_etc1_differential_color2_offset_random_bits_table[generation_table_index];
			else
				offset_random_bits =
					detex_etc1_codeword_offset_random_bits_table[generation_table_index];
			int rnd = rng->RandomBits(offset_random_bits + 1);
			sign_bit = rnd & 1;
			offset = (rnd >> 1) + 1;
			// Apply positive or negative displacement.
			if (sign_bit == 0) {
				value += offset;
				if (value > detex_etc1_differential_component_max_value[component])
					value = detex_etc1_differential_component_max_value[component];
			}
			else {
				value -= offset;
				if (value < 0)
					value = 0;
			}
			colors &= ~mask;
			colors |= value << detex_etc1_differential_component_shift[component];
		}
		if (!ColorsAreInvalidETC1Differential(colors))
			break;
	}
	*(uint32_t *)bitstring32 = colors;
}

void MutateETC1(const detexBlockInfo * DETEX_RESTRICT info, dstCMWCRNG * DETEX_RESTRICT rng, int generation,
uint8_t * DETEX_RESTRICT bitstring) {
	if (bitstring[3] & 2)
		return MutateETC1Differential(info, rng, generation, bitstring);
	else
		return MutateETC1Individual(info, rng, generation, bitstring);
}

static const int modifier_table[8][4] = {
	{ 2, 8, -2, -8 },
	{ 5, 17, -5, -17 },
	{ 9, 29, -9, -29 },
	{ 13, 42, -13, -42 },
	{ 18, 60, -18, -60 },
	{ 24, 80, -24, -80 },
	{ 33, 106, -33, -106 },
	{ 47, 183, -47, -183 }
};

static DETEX_INLINE_ONLY uint32_t GetPixelErrorETC1(int r_orig, int g_orig, int b_orig, uint32_t table_codeword,
int *base_color, int pixel_index) {
	int modifier = modifier_table[table_codeword][pixel_index];
	int r = detexClamp0To255(base_color[0] + modifier);
	int g = detexClamp0To255(base_color[1] + modifier);
	int b = detexClamp0To255(base_color[2] + modifier);
	return GetPixelErrorRGB8(r_orig, g_orig, b_orig, r, g, b);
}

static DETEX_INLINE_ONLY uint32_t SetPixelXYETC1(const uint8_t * DETEX_RESTRICT pix_orig, int stride_orig, int dx, int dy,
uint32_t table_codeword, int * DETEX_RESTRICT base_color, uint32_t & DETEX_RESTRICT pixel_indices) {
	uint32_t pixel_orig = *(uint32_t *)(pix_orig + dy * stride_orig + dx * 4);
	int r_orig = detexPixel32GetR8(pixel_orig);
	int g_orig = detexPixel32GetG8(pixel_orig);
	int b_orig = detexPixel32GetB8(pixel_orig);
	uint32_t best_error = GetPixelErrorETC1(r_orig, g_orig, b_orig, table_codeword, base_color, 0);
	int best_pixel_index = 0;
	uint32_t error1 = GetPixelErrorETC1(r_orig, g_orig, b_orig, table_codeword, base_color, 1);
	if (error1 < best_error) {
		best_error = error1;
		best_pixel_index = 1;
	}
	uint32_t error2 = GetPixelErrorETC1(r_orig, g_orig, b_orig, table_codeword, base_color, 2);
	if (error2 < best_error) {
		best_error = error2;
		best_pixel_index = 2;
	}
	uint32_t error3 = GetPixelErrorETC1(r_orig, g_orig, b_orig, table_codeword, base_color, 3);
	if (error3 < best_error) {
		best_error = error3;
		best_pixel_index = 3;
	}
	int i = dy + dx * 4;
	pixel_indices |= (best_pixel_index & 1) << i;
	pixel_indices |= (best_pixel_index & 2) << (i + 15);
	return best_error;
}

static DETEX_INLINE_ONLY void DecodeColorsETC1ModeIndividual(uint32_t colors,
int * DETEX_RESTRICT base_color_subblock1, int * DETEX_RESTRICT base_color_subblock2,
int * DETEX_RESTRICT table_codeword) {
	base_color_subblock1[0] = colors & 0xF0;
	base_color_subblock1[0] |= base_color_subblock1[0] >> 4;
	base_color_subblock1[1] = (colors >> 8) & 0xF0;
	base_color_subblock1[1] |= base_color_subblock1[1] >> 4;
	base_color_subblock1[2] = (colors >> 16) & 0xF0;
	base_color_subblock1[2] |= base_color_subblock1[2] >> 4;
	base_color_subblock2[0] = colors & 0x0F;
	base_color_subblock2[0] |= base_color_subblock2[0] << 4;
	base_color_subblock2[1] = (colors >> 8) & 0x0F;
	base_color_subblock2[1] |= base_color_subblock2[1] << 4;
	base_color_subblock2[2] = (colors >> 16) & 0x0F;
	base_color_subblock2[2] |= base_color_subblock2[2] << 4;
	table_codeword[0] = colors >> 29;
	table_codeword[1] = (colors >> 26) & 0x7;
}

static DETEX_INLINE_ONLY void DecodeColorsETC1ModeDifferential(uint32_t colors,
int * DETEX_RESTRICT base_color_subblock1, int * DETEX_RESTRICT base_color_subblock2,
int * DETEX_RESTRICT table_codeword) {
	base_color_subblock1[0] = colors & 0xF8;
	base_color_subblock1[0] |= ((base_color_subblock1[0] & 224) >> 5);
	base_color_subblock1[1] = (colors >> 8) & 0xF8;
	base_color_subblock1[1] |= (base_color_subblock1[1] & 224) >> 5;
	base_color_subblock1[2] = (colors >> 16) & 0xF8;
	base_color_subblock1[2] |= (base_color_subblock1[2] & 224) >> 5;
	base_color_subblock2[0] = colors & 0xF8;			// 5 highest order bits.
	base_color_subblock2[0] += complement3bitshifted(colors & 7);	// Add difference.
//	if (base_color_subblock2[0] & 0xFF07)				// Check for overflow.
//		return false;
	base_color_subblock2[0] |= (base_color_subblock2[0] & 224) >> 5;	// Replicate.
	base_color_subblock2[1] = (colors >> 8) & 0xF8;
	base_color_subblock2[1] += complement3bitshifted((colors >> 8) & 7);
//	if (base_color_subblock2[1] & 0xFF07)
//		return false;
	base_color_subblock2[1] |= (base_color_subblock2[1] & 224) >> 5;
	base_color_subblock2[2] = (colors >> 16) & 0xF8;
	base_color_subblock2[2] += complement3bitshifted((colors >> 16) & 7);
//	if (base_color_subblock2[2] & 0xFF07)
//		return false;
	base_color_subblock2[2] |= (base_color_subblock2[2] & 224) >> 5;
	table_codeword[0] = colors >> 29;
	table_codeword[1] = (colors >> 26) & 0x7;
}

static uint32_t SetPixelsETC1ModeIndividualFlipBit0(const detexBlockInfo * DETEX_RESTRICT info,
uint8_t * DETEX_RESTRICT bitstring) {
	// Decode colors.
	uint32_t colors = *(uint32_t *)&bitstring[0];
	// Decode the two base colors.
	int base_color_subblock1[3];
	int base_color_subblock2[3];
	int table_codeword[2];
	DecodeColorsETC1ModeIndividual(colors, base_color_subblock1, base_color_subblock2, table_codeword);
	// Set pixels indices.
	const detexTexture *texture = info->texture;
	int x = info->x;
	int y = info->y;
	uint8_t *pix_orig = texture->data + (y * texture->width + x) * 4;
	int stride_orig = texture->width * 4;
	uint32_t pixel_indices = 0;
	uint32_t error = 0;
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 0, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 1, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 2, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 3, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 0, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 1, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 2, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 3, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 0, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 1, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 2, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 3, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 0, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 1, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 2, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 3, table_codeword[1], base_color_subblock2, pixel_indices);
	bitstring[4] = pixel_indices >> 24;
	bitstring[5] = pixel_indices >> 16;
	bitstring[6] = pixel_indices >> 8;
	bitstring[7] = pixel_indices;
	return error;
}

static uint32_t SetPixelsETC1ModeIndividualFlipBit1(const detexBlockInfo * DETEX_RESTRICT info,
uint8_t * DETEX_RESTRICT bitstring) {
	// Decode colors.
	uint32_t colors = *(uint32_t *)&bitstring[0];
	// Decode the two base colors.
	int base_color_subblock1[3];
	int base_color_subblock2[3];
	int table_codeword[2];
	DecodeColorsETC1ModeIndividual(colors, base_color_subblock1, base_color_subblock2, table_codeword);
	// Set pixels indices.
	const detexTexture *texture = info->texture;
	int x = info->x;
	int y = info->y;
	uint8_t *pix_orig = texture->data + (y * texture->width + x) * 4;
	int stride_orig = texture->width * 4;
	uint32_t pixel_indices = 0;
	uint32_t error = 0;
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 0, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 1, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 2, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 3, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 0, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 1, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 2, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 3, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 0, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 1, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 2, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 3, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 0, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 1, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 2, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 3, table_codeword[1], base_color_subblock2, pixel_indices);
	bitstring[4] = pixel_indices >> 24;
	bitstring[5] = pixel_indices >> 16;
	bitstring[6] = pixel_indices >> 8;
	bitstring[7] = pixel_indices;
	return error;
}

static uint32_t SetPixelsETC1ModeDifferentialFlipBit0(const detexBlockInfo * DETEX_RESTRICT info,
uint8_t * DETEX_RESTRICT bitstring) {
	// Decode colors.
	uint32_t colors = *(uint32_t *)&bitstring[0];
	// Decode the two base colors.
	int base_color_subblock1[3];
	int base_color_subblock2[3];
	int table_codeword[2];
	DecodeColorsETC1ModeDifferential(colors, base_color_subblock1, base_color_subblock2, table_codeword);
	// Set pixels indices.
	const detexTexture *texture = info->texture;
	int x = info->x;
	int y = info->y;
	uint8_t *pix_orig = texture->data + (y * texture->width + x) * 4;
	int stride_orig = texture->width * 4;
	uint32_t pixel_indices = 0;
	uint32_t error = 0;
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 0, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 1, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 2, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 3, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 0, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 1, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 2, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 3, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 0, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 1, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 2, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 3, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 0, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 1, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 2, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 3, table_codeword[1], base_color_subblock2, pixel_indices);
	bitstring[4] = pixel_indices >> 24;
	bitstring[5] = pixel_indices >> 16;
	bitstring[6] = pixel_indices >> 8;
	bitstring[7] = pixel_indices;
	return error;
}

static uint32_t SetPixelsETC1ModeDifferentialFlipBit1(const detexBlockInfo * DETEX_RESTRICT info,
uint8_t * DETEX_RESTRICT bitstring) {
	// Decode colors.
	uint32_t colors = *(uint32_t *)&bitstring[0];
	// Decode the two base colors.
	int base_color_subblock1[3];
	int base_color_subblock2[3];
	int table_codeword[2];
	DecodeColorsETC1ModeDifferential(colors, base_color_subblock1, base_color_subblock2, table_codeword);
	// Set pixels indices.
	const detexTexture *texture = info->texture;
	int x = info->x;
	int y = info->y;
	uint8_t *pix_orig = texture->data + (y * texture->width + x) * 4;
	int stride_orig = texture->width * 4;
	uint32_t pixel_indices = 0;
	uint32_t error = 0;
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 0, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 1, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 2, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 0, 3, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 0, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 1, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 2, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 1, 3, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 0, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 1, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 2, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 2, 3, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 0, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 1, table_codeword[0], base_color_subblock1, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 2, table_codeword[1], base_color_subblock2, pixel_indices);
	error += SetPixelXYETC1(pix_orig, stride_orig, 3, 3, table_codeword[1], base_color_subblock2, pixel_indices);
	bitstring[4] = pixel_indices >> 24;
	bitstring[5] = pixel_indices >> 16;
	bitstring[6] = pixel_indices >> 8;
	bitstring[7] = pixel_indices;
	return error;
}

// Set the pixel indices of the compressed block using the available colors so that they
// most closely match the original block. Return the comparison error value.
uint32_t SetPixelsETC1(const detexBlockInfo * DETEX_RESTRICT info, uint8_t * DETEX_RESTRICT bitstring) {
	int ETC1_mode = bitstring[3] & 3;
	switch (ETC1_mode) {
	case 0 :
		return SetPixelsETC1ModeIndividualFlipBit0(info, bitstring);
	case 1 :
		return SetPixelsETC1ModeIndividualFlipBit1(info, bitstring);
	case 2 :
		return SetPixelsETC1ModeDifferentialFlipBit0(info, bitstring);
	case 3 :
		return SetPixelsETC1ModeDifferentialFlipBit1(info, bitstring);
	}
}

