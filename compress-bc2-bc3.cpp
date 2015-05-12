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

void SeedBC2(const detexTexture *texture, int x, int y, dstCMWCRNG * DST_RESTRICT rng, int mode,
uint32_t flags, uint32_t * DST_RESTRICT colors, uint8_t * DST_RESTRICT bitstring) {
	// Only seed the color values.
	uint32_t color_values = rng->Random32();
	uint32_t *color_values_p32 = (uint32_t *)(bitstring + 8);
	*(uint32_t *)color_values_p32 = color_values;
	// Ensure the colors correspond to mode 0.
	detexSetModeBC1(bitstring + 8, 0, 0, NULL);
	SetAlphaPixelsBC2(texture, x, y, bitstring);
}

void MutateBC2(dstCMWCRNG * DST_RESTRICT rng, int generation, int mode,
uint8_t * DST_RESTRICT bitstring) {
	MutateBC1(rng, generation, mode, bitstring + 8);
}

uint32_t SetPixelsBC2(const detexTexture * DETEX_RESTRICT texture, int x, int y,
uint8_t * DETEX_RESTRICT bitstring) {
	return SetPixelsBC1(texture, x, y, bitstring + 8);
}

void MutateBC3(dstCMWCRNG * DST_RESTRICT rng, int generation, int mode,
uint8_t * DST_RESTRICT bitstring) {
	MutateBC1(rng, generation, mode, bitstring + 8);
}

uint32_t SetPixelsBC3(const detexTexture * DETEX_RESTRICT texture, int x, int y,
uint8_t * DETEX_RESTRICT bitstring) {
	uint32_t error = SetPixelsBC1(texture, x, y, bitstring + 8);
	uint8_t *pix_orig = texture->data + (y * texture->width + x) * 4;
	int stride_orig = texture->width * 4;
	return error;
}

