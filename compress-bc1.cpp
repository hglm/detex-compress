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

static const uint32_t detex_bc1_component_mask[6] = {
	0x0000001F, 0x000007E0, 0x0000F800, 0x001F0000, 0x07E00000, 0xF8000000
};

static const uint8_t detex_bc1_component_shift[6] = {
	0, 5, 11, 16, 21, 27
};

static const int detex_bc1_component_max_value[6] = {
	31, 63, 31, 31, 63, 31
};

static const int8_t detex_bc1_mutation_table1[16][8] = {
//	{ 0, -1, 0, 0, 0, 0, 0, 0 },	// red1
//	{ 1, -1, 0, 0, 0, 0, 0, 0 },	// green1
//	{ 2, -1, 0, 0, 0, 0, 0, 0 },	// blue1
//	{ 3, -1, 0, 0, 0, 0, 0, 0 },	// red2
//	{ 4, -1, 0, 0, 0, 0, 0, 0 },	// green2
//	{ 5, -1, 0, 0, 0, 0, 0, 0 },	// blue2
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
	{ 0, 1, 2, -1, 0, 0, 0, 0 },	// red1, green1, blue1
	{ 3, 4, 5, -1, 0, 0, 0, 0 },	// red2, green2, blue2
	{ 3, 4, 5, -1, 0, 0, 0, 0 },	// red2, green2, blue2
	{ 3, 4, 5, -1, 0, 0, 0, 0 },	// red2, green2, blue2
	{ 0, 1, 2, 3, 4, 5, -1, 0 },	// red1, green1, blue1, red2, green2, blue2
//	{ 0, 1, 2, 3, 4, 5, -1, 0 },	// red1, green1, blue1, red2, green2, blue2
};

static const int8_t detex_bc1_mutation_table2[16][8] = {
	{ 0, -1, 0, 0, 0, 0, 0, 0 },	// red1
	{ 1, -1, 0, 0, 0, 0, 0, 0 },	// green1
	{ 2, -1, 0, 0, 0, 0, 0, 0 },	// blue1
	{ 3, -1, 0, 0, 0, 0, 0, 0 },	// red2
	{ 4, -1, 0, 0, 0, 0, 0, 0 },	// green2
	{ 5, -1, 0, 0, 0, 0, 0, 0 },	// blue2
	{ 0, 3, -1, 0, 0, 0, 0, 0 },	// red1, red2
	{ 0, 3, -1, 0, 0, 0, 0, 0 },	// red1, red2
	{ 1, 4, -1, 0, 0, 0, 0, 0 },	// green1, green2
	{ 1, 4, -1, 0, 0, 0, 0, 0 },	// green1, green2
	{ 2, 5, -1, 0, 0, 0, 0, 0 },	// blue1, blue2
	{ 2, 5, -1, 0, 0, 0, 0, 0 },	// blue1, blue2
	{ 0, 1, 2, -1, 0, 0, 0, 0 },	// red1, green1, blue1
	{ 0, 1, 2, -1, 0, 0, 0, 0 },	// red1, green1, blue1
	{ 3, 4, 5, -1, 0, 0, 0, 0 },	// red2, green2, blue2
	{ 3, 4, 5, -1, 0, 0, 0, 0 },	// red2, green2, blue2
};

static const uint8_t detex_bc1_offset_random_bits_table[] = {
	4, 4, 4, 4, 4, 4, 4, 4,	// Random 1 to 16, generation 128-1023 (unused)
	4,			// Random 1 to 16, generation 1024-1151
	3,			// Random 1 to 8, generation 1152-1279
	2, 2,			// Random 1 to 4, generation 1280-1535
	1, 1, 1, 1,		// Random 1 to 2, generation 1536-2043
};

void SeedBC1(const detexBlockInfo * DETEX_RESTRICT info, dstCMWCRNG *rng, uint8_t * DETEX_RESTRICT bitstring) {
	// Only need to initialize the color fields. The pixel values will be set later.
	uint32_t *bitstring32 = (uint32_t *)bitstring;
	*(uint32_t *)bitstring32 = rng->Random32();
	if (info->mode >= 0)
		detexSetModeBC1(bitstring, info->mode, 0, NULL);
}

void MutateBC1(const detexBlockInfo * DETEX_RESTRICT info, dstCMWCRNG * DETEX_RESTRICT rng, int generation,
uint8_t * DETEX_RESTRICT bitstring) {
	uint32_t *bitstring32 = (uint32_t *)bitstring;
	uint32_t colors = *bitstring32;
	if (generation < 1024 /* || (generation & 7) == 0 */) {
		// For generations < 1024, replace components entirely with a random
		// value.
		for (;;) {
			int mutation_type = rng->RandomBits(4);
			const int8_t *mutationp = detex_bc1_mutation_table1[mutation_type];
			for (;*mutationp >= 0; mutationp++) {
				int component = *mutationp;
				uint32_t mask = detex_bc1_component_mask[component];
				// Set the component to a random value.
				int value = rng->RandomBits(6) & detex_bc1_component_max_value[component];
				colors &= ~mask;
				colors |= value << detex_bc1_component_shift[component];
			}
			int m = (colors & 0xFFFF) <= ((colors & 0xFFFF0000) >> 16);
			if (info->mode < 0 || m == info->mode)
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
		const int8_t *mutationp = detex_bc1_mutation_table2[mutation_type];
		for (;*mutationp >= 0; mutationp++) {
			int component = *mutationp;
			uint32_t mask = detex_bc1_component_mask[component];
			int value = (colors & mask) >> detex_bc1_component_shift[component];
			int offset;
			int sign_bit;
			int offset_random_bits = detex_bc1_offset_random_bits_table[generation_table_index];
			offset_random_bits <<= (component == 1 || component == 4);
			int rnd = rng->RandomBits(offset_random_bits + 1);
			sign_bit = rnd & 1;
			offset = (rnd >> 1) + 1;
			// Apply positive or negative displacement.
			if (sign_bit == 0) {
				value += offset;
				if (value > detex_bc1_component_max_value[component])
					value = detex_bc1_component_max_value[component];
			}
			else {
				value -= offset;
				if (value < 0)
					value = 0;
			}
			colors &= ~mask;
			colors |= value << detex_bc1_component_shift[component];
		}
		int m = (colors & 0xFFFF) <= ((colors & 0xFFFF0000) >> 16);
		if (info->mode < 0 || m == info->mode)
			break;
	}
	*(uint32_t *)bitstring32 = colors;
}

static DETEX_INLINE_ONLY uint32_t SetPixelXYBC1(const uint8_t * DETEX_RESTRICT pix_orig, int stride_orig, int dx, int dy,
const int * DETEX_RESTRICT color_r, const int * DETEX_RESTRICT color_g, const int * DETEX_RESTRICT color_b,
uint32_t & DETEX_RESTRICT pixel_indices) {
	uint32_t pixel_orig = *(uint32_t *)(pix_orig + dy * stride_orig + dx * 4);
	int r_orig = detexPixel32GetR8(pixel_orig);
	int g_orig = detexPixel32GetG8(pixel_orig);
	int b_orig = detexPixel32GetB8(pixel_orig);
#ifdef __SSE2__
	__simd128_int m_color_orig_r = simd128_set_same_int32(r_orig);
	__simd128_int m_color_orig_g = simd128_set_same_int32(g_orig);
	__simd128_int m_color_orig_b = simd128_set_same_int32(b_orig);
	__simd128_int m_color_r = simd128_load_int(color_r);
	__simd128_int m_color_g = simd128_load_int(color_g);
	__simd128_int m_color_b = simd128_load_int(color_b);
	__simd128_int m_diff_r = simd128_sub_int32(m_color_orig_r, m_color_r);
	__simd128_int m_diff_g = simd128_sub_int32(m_color_orig_g, m_color_g);
	__simd128_int m_diff_b = simd128_sub_int32(m_color_orig_b, m_color_b);
#ifdef __SSE41__
	__simd128_int m_sqr_diff_r = _mm_mullo_epi32(m_diff_r, m_diff_r);
	__simd128_int m_sqr_diff_g = _mm_mullo_epi32(m_diff_g, m_diff_g);
	__simd128_int m_sqr_diff_b = _mm_mullo_epi32(m_diff_b, m_diff_b);
#else
	__simd128_int low_int16_mask = simd128_set_same_int32(0x0000FFFF);
	__simd128_int m_sqr_diff_r = simd128_and_int(
		_mm_mullo_epi16(m_diff_r, m_diff_r),
		low_int16_mask);
	__simd128_int m_sqr_diff_g = simd128_and_int(
		_mm_mullo_epi16(m_diff_g, m_diff_g),
		low_int16_mask);
	__simd128_int m_sqr_diff_b = simd128_and_int(
		_mm_mullo_epi16(m_diff_b, m_diff_b),
		low_int16_mask);
#endif
	__simd128_int m_error_pixel0123 = simd128_add_int32(m_sqr_diff_r,
		simd128_add_int32(m_sqr_diff_g, m_sqr_diff_b));
#if 0
	__simd128_int m_best_error = m_error_pixel0123;
	__simd128_int m_best_pixel_index = simd128_set_zero_int();
	// Compare error1 with error0
	__simd128_int m_error = simd128_shift_right_bytes_int(m_error_pixel0123, 4);
	__simd128_int m_cmp = _mm_cmplt_epi32(m_error, m_best_error);
	m_best_pixel_index = simd128_or_int(
		m_best_pixel_index,
		simd128_and_int(
			simd128_set_same_int32(1), m_cmp)
		);
	m_best_error = simd128_or_int(
		simd128_andnot_int(m_cmp, m_best_error),
		simd128_and_int(m_cmp, m_error));
	// Compare error2
	m_error = simd128_shift_right_bytes_int(m_error_pixel0123, 8);
	m_cmp = _mm_cmplt_epi32(m_error, m_best_error);
	m_best_pixel_index = simd128_or_int(
		simd128_andnot_int(
			m_cmp, m_best_pixel_index),
		simd128_and_int(
			m_cmp, simd128_set_same_int32(2))
		);
	m_best_error = simd128_or_int(
		simd128_andnot_int(m_cmp, m_best_error),
		simd128_and_int(m_cmp, m_error));
	// Compare error3
	m_error = simd128_shift_right_bytes_int(m_error_pixel0123, 12);
	m_cmp = _mm_cmplt_epi32(m_error, m_best_error);
	m_best_pixel_index = simd128_or_int(
		simd128_andnot_int(
			m_cmp, m_best_pixel_index),
		simd128_and_int(
			m_cmp, simd128_set_same_int32(3))
		);
	m_best_error = simd128_or_int(
		simd128_andnot_int(m_cmp, m_best_error),
		simd128_and_int(m_cmp, m_error));
	uint32_t best_pixel_index = simd128_get_int32(m_best_pixel_index);
	uint32_t best_error = simd128_get_int32(m_best_error);
//	printf("best_pixel_index = %d, best_error = 0x%08X\n", best_pixel_index, best_error);
#else
	uint32_t best_error = simd128_get_int32(m_error_pixel0123);
	uint32_t error1 = simd128_get_int32(simd128_shift_right_bytes_int(m_error_pixel0123, 4));
	uint32_t error2 = simd128_get_int32(simd128_shift_right_bytes_int(m_error_pixel0123, 8));
	uint32_t error3 = simd128_get_int32(simd128_shift_right_bytes_int(m_error_pixel0123, 12));
// printf("Error0/1/2/3: 0x%08X, 0x%08X, 0x%08X, 0x%08X\n", best_error, error1, error2, error3);
	int best_pixel_index = 0;
	if (error1 < best_error) {
		best_error = error1;
		best_pixel_index = 1;
	}
	if (error2 < best_error) {
		best_error = error2;
		best_pixel_index = 2;
	}
	if (error3 < best_error) {
		best_error = error3;
		best_pixel_index = 3;
	}
#endif
#else
	uint32_t best_error = GetPixelErrorRGB8(r_orig, g_orig, b_orig, color_r[0], color_g[0], color_b[0]);
	int best_pixel_index = 0;
	uint32_t error1 = GetPixelErrorRGB8(r_orig, g_orig, b_orig, color_r[1], color_g[1], color_b[1]);
	if (error1 < best_error) {
		best_error = error1;
		best_pixel_index = 1;
	}
	uint32_t error2 = GetPixelErrorRGB8(r_orig, g_orig, b_orig, color_r[2], color_g[2], color_b[2]);
	if (error2 < best_error) {
		best_error = error2;
		best_pixel_index = 2;
	}
	uint32_t error3 = GetPixelErrorRGB8(r_orig, g_orig, b_orig, color_r[3], color_g[3], color_b[3]);
	if (error3 < best_error) {
		best_error = error3;
		best_pixel_index = 3;
	}
#endif
	int i = dy * 4 + dx;
	pixel_indices |= best_pixel_index << (i * 2);
	return best_error;
}

static DETEX_INLINE_ONLY void DecodeColorsBC1(uint32_t colors, int * DETEX_RESTRICT color_r,
int * DETEX_RESTRICT color_g, int * DETEX_RESTRICT color_b) {
	color_b[0] = (colors & 0x0000001F) << 3;
	color_g[0] = (colors & 0x000007E0) >> (5 - 2);
	color_r[0] = (colors & 0x0000F800) >> (11 - 3);
	color_b[1] = (colors & 0x001F0000) >> (16 - 3);
	color_g[1] = (colors & 0x07E00000) >> (21 - 2);
	color_r[1] = (colors & 0xF8000000) >> (27 - 3);
	if ((colors & 0xFFFF) > ((colors & 0xFFFF0000) >> 16)) {
		color_r[2] = detexDivide0To767By3(2 * color_r[0] + color_r[1]);
		color_g[2] = detexDivide0To767By3(2 * color_g[0] + color_g[1]);
		color_b[2] = detexDivide0To767By3(2 * color_b[0] + color_b[1]);
		color_r[3] = detexDivide0To767By3(color_r[0] + 2 * color_r[1]);
		color_g[3] = detexDivide0To767By3(color_g[0] + 2 * color_g[1]);
		color_b[3] = detexDivide0To767By3(color_b[0] + 2 * color_b[1]);
	}
	else {
		color_r[2] = (color_r[0] + color_r[1]) / 2;
		color_g[2] = (color_g[0] + color_g[1]) / 2;
		color_b[2] = (color_b[0] + color_b[1]) / 2;
		color_r[3] = color_g[3] = color_b[3] = 0;
	}
}

// Set the pixel indices of the compressed block using the available colors so that they
// most closely match the original block. Return the comparison error value.
uint32_t SetPixelsBC1(const detexBlockInfo * DETEX_RESTRICT info, uint8_t * DETEX_RESTRICT bitstring) {
	// Decode colors.
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || !defined(__BYTE_ORDER__)
	uint32_t colors = *(uint32_t *)&bitstring[0];
#else
	uint32_t colors = ((uint32_t)bitstring[0] << 24) |
		((uint32_t)bitstring[1] << 16) |
		((uint32_t)bitstring[2] << 8) | bitstring[3];
#endif
	// Decode the two 5-6-5 RGB colors.
	int color_r[4] DST_ALIGNED(16), color_g[4] DST_ALIGNED(16), color_b[4] DST_ALIGNED(16);
	DecodeColorsBC1(colors, color_r, color_g, color_b);
	// Set pixels indices.
	const detexTexture *texture = info->texture;
	int x = info->x;
	int y = info->y;
	uint8_t *pix_orig = texture->data + (y * texture->width + x) * 4;
	int stride_orig = texture->width * 4;
	uint32_t pixel_indices = 0;
	uint32_t error = 0;
	error += SetPixelXYBC1(pix_orig, stride_orig, 0, 0, color_r, color_g, color_b, pixel_indices);
	error += SetPixelXYBC1(pix_orig, stride_orig, 1, 0, color_r, color_g, color_b, pixel_indices);
	error += SetPixelXYBC1(pix_orig, stride_orig, 2, 0, color_r, color_g, color_b, pixel_indices);
	error += SetPixelXYBC1(pix_orig, stride_orig, 3, 0, color_r, color_g, color_b, pixel_indices);
	error += SetPixelXYBC1(pix_orig, stride_orig, 0, 1, color_r, color_g, color_b, pixel_indices);
	error += SetPixelXYBC1(pix_orig, stride_orig, 1, 1, color_r, color_g, color_b, pixel_indices);
	error += SetPixelXYBC1(pix_orig, stride_orig, 2, 1, color_r, color_g, color_b, pixel_indices);
	error += SetPixelXYBC1(pix_orig, stride_orig, 3, 1, color_r, color_g, color_b, pixel_indices);
	error += SetPixelXYBC1(pix_orig, stride_orig, 0, 2, color_r, color_g, color_b, pixel_indices);
	error += SetPixelXYBC1(pix_orig, stride_orig, 1, 2, color_r, color_g, color_b, pixel_indices);
	error += SetPixelXYBC1(pix_orig, stride_orig, 2, 2, color_r, color_g, color_b, pixel_indices);
	error += SetPixelXYBC1(pix_orig, stride_orig, 3, 2, color_r, color_g, color_b, pixel_indices);
	error += SetPixelXYBC1(pix_orig, stride_orig, 0, 3, color_r, color_g, color_b, pixel_indices);
	error += SetPixelXYBC1(pix_orig, stride_orig, 1, 3, color_r, color_g, color_b, pixel_indices);
	error += SetPixelXYBC1(pix_orig, stride_orig, 2, 3, color_r, color_g, color_b, pixel_indices);
	error += SetPixelXYBC1(pix_orig, stride_orig, 3, 3, color_r, color_g, color_b, pixel_indices);
	*(uint32_t *)(bitstring + 4) = pixel_indices;
	return error;
}

static const int detex_BC1A_modes_1[] = { 1, -1 };
static const int detex_BC1A_modes_01[] = { 0, 1, -1 };

const int *GetModesBC1A(const detexBlockInfo *info) {
	if ((info->flags & DETEX_BLOCK_FLAG_OPAQUE) == 0)
		// If the block has non-opaque pixels, use mode 1.
		return detex_BC1A_modes_1;
	return detex_BC1A_modes_01;
}

static DETEX_INLINE_ONLY void DecodeColorsBC1A(uint32_t colors, int * DETEX_RESTRICT color_r,
int * DETEX_RESTRICT color_g, int * DETEX_RESTRICT color_b, int * DETEX_RESTRICT color_a) {
	color_b[0] = (colors & 0x0000001F) << 3;
	color_g[0] = (colors & 0x000007E0) >> (5 - 2);
	color_r[0] = (colors & 0x0000F800) >> (11 - 3);
	color_b[1] = (colors & 0x001F0000) >> (16 - 3);
	color_g[1] = (colors & 0x07E00000) >> (21 - 2);
	color_r[1] = (colors & 0xF8000000) >> (27 - 3);
	color_a[0] = color_a[1] = color_a[2] = 0xFF;
	if ((colors & 0xFFFF) > ((colors & 0xFFFF0000) >> 16)) {
		color_r[2] = detexDivide0To767By3(2 * color_r[0] + color_r[1]);
		color_g[2] = detexDivide0To767By3(2 * color_g[0] + color_g[1]);
		color_b[2] = detexDivide0To767By3(2 * color_b[0] + color_b[1]);
		color_r[3] = detexDivide0To767By3(color_r[0] + 2 * color_r[1]);
		color_g[3] = detexDivide0To767By3(color_g[0] + 2 * color_g[1]);
		color_b[3] = detexDivide0To767By3(color_b[0] + 2 * color_b[1]);
		color_a[3] = 0xFF;
	}
	else {
		color_r[2] = (color_r[0] + color_r[1]) / 2;
		color_g[2] = (color_g[0] + color_g[1]) / 2;
		color_b[2] = (color_b[0] + color_b[1]) / 2;
		color_r[3] = color_g[3] = color_b[3] = 0;
		color_a[3] = 0;
	}
}

static DETEX_INLINE_ONLY uint32_t SetPixelXYBC1A(const uint8_t * DETEX_RESTRICT pix_orig, int stride_orig, int dx, int dy,
const int * DETEX_RESTRICT color_r, const int * DETEX_RESTRICT color_g, const int * DETEX_RESTRICT color_b,
const int * DETEX_RESTRICT color_a, uint32_t & DETEX_RESTRICT pixel_indices) {
	uint32_t pixel_orig = *(uint32_t *)(pix_orig + dy * stride_orig + dx * 4);
	int r_orig = detexPixel32GetR8(pixel_orig);
	int g_orig = detexPixel32GetG8(pixel_orig);
	int b_orig = detexPixel32GetB8(pixel_orig);
	int a_orig = detexPixel32GetA8(pixel_orig);
	uint32_t best_error = GetPixelErrorRGBA8(r_orig, g_orig, b_orig, a_orig, color_r[0], color_g[0], color_b[0],
		color_a[0]);
	int best_pixel_index = 0;
	uint32_t error1 = GetPixelErrorRGBA8(r_orig, g_orig, b_orig, a_orig, color_r[1], color_g[1], color_b[1],
		color_a[1]);
	if (error1 < best_error) {
		best_error = error1;
		best_pixel_index = 1;
	}
	uint32_t error2 = GetPixelErrorRGBA8(r_orig, g_orig, b_orig, a_orig, color_r[2], color_g[2], color_b[2],
		color_a[2]);
	if (error2 < best_error) {
		best_error = error2;
		best_pixel_index = 2;
	}
	uint32_t error3 = GetPixelErrorRGBA8(r_orig, g_orig, b_orig, a_orig, color_r[3], color_g[3], color_b[3],
		color_a[3]);
	if (error3 < best_error) {
		best_error = error3;
		best_pixel_index = 3;
	}
	int i = dy * 4 + dx;
	pixel_indices |= best_pixel_index << (i * 2);
	return best_error;
}

// Set the pixel indices of the compressed block using the available colors so that they
// most closely match the original block. Return the comparison error value.
uint32_t SetPixelsBC1A(const detexBlockInfo * DETEX_RESTRICT info, uint8_t * DETEX_RESTRICT bitstring) {
	// Decode colors.
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || !defined(__BYTE_ORDER__)
	uint32_t colors = *(uint32_t *)&bitstring[0];
#else
	uint32_t colors = ((uint32_t)bitstring[0] << 24) |
		((uint32_t)bitstring[1] << 16) |
		((uint32_t)bitstring[2] << 8) | bitstring[3];
#endif
	// Decode the two 5-6-5 RGB colors.
	int color_r[4], color_g[4], color_b[4], color_a[4];
	DecodeColorsBC1A(colors, color_r, color_g, color_b, color_a);
	// Set pixels indices.
	const detexTexture *texture = info->texture;
	int x = info->x;
	int y = info->y;
	uint8_t *pix_orig = texture->data + (y * texture->width + x) * 4;
	int stride_orig = texture->width * 4;
	uint32_t pixel_indices = 0;
	uint32_t error = 0;
	error += SetPixelXYBC1A(pix_orig, stride_orig, 0, 0, color_r, color_g, color_b, color_a, pixel_indices);
	error += SetPixelXYBC1A(pix_orig, stride_orig, 1, 0, color_r, color_g, color_b, color_a, pixel_indices);
	error += SetPixelXYBC1A(pix_orig, stride_orig, 2, 0, color_r, color_g, color_b, color_a, pixel_indices);
	error += SetPixelXYBC1A(pix_orig, stride_orig, 3, 0, color_r, color_g, color_b, color_a, pixel_indices);
	error += SetPixelXYBC1A(pix_orig, stride_orig, 0, 1, color_r, color_g, color_b, color_a, pixel_indices);
	error += SetPixelXYBC1A(pix_orig, stride_orig, 1, 1, color_r, color_g, color_b, color_a, pixel_indices);
	error += SetPixelXYBC1A(pix_orig, stride_orig, 2, 1, color_r, color_g, color_b, color_a, pixel_indices);
	error += SetPixelXYBC1A(pix_orig, stride_orig, 3, 1, color_r, color_g, color_b, color_a, pixel_indices);
	error += SetPixelXYBC1A(pix_orig, stride_orig, 0, 2, color_r, color_g, color_b, color_a, pixel_indices);
	error += SetPixelXYBC1A(pix_orig, stride_orig, 1, 2, color_r, color_g, color_b, color_a, pixel_indices);
	error += SetPixelXYBC1A(pix_orig, stride_orig, 2, 2, color_r, color_g, color_b, color_a, pixel_indices);
	error += SetPixelXYBC1A(pix_orig, stride_orig, 3, 2, color_r, color_g, color_b, color_a, pixel_indices);
	error += SetPixelXYBC1A(pix_orig, stride_orig, 0, 3, color_r, color_g, color_b, color_a, pixel_indices);
	error += SetPixelXYBC1A(pix_orig, stride_orig, 1, 3, color_r, color_g, color_b, color_a, pixel_indices);
	error += SetPixelXYBC1A(pix_orig, stride_orig, 2, 3, color_r, color_g, color_b, color_a, pixel_indices);
	error += SetPixelXYBC1A(pix_orig, stride_orig, 3, 3, color_r, color_g, color_b, color_a, pixel_indices);
	*(uint32_t *)(bitstring + 4) = pixel_indices;
	return error;
}

