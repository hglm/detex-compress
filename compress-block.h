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

// Definitions for block compression modules.

enum {
	/* The block is opaque (alpha is always 0xFF). */
	DETEX_BLOCK_FLAG_OPAQUE = 0x2,
	/* The block is non-opaque (alpha is not always 0xFF). */
	DETEX_BLOCK_FLAG_NON_OPAQUE = 0x4,
	/* The block has punchthrough alpha (alpha is either 0x00 or 0xFF). */
	DETEX_BLOCK_FLAG_PUNCHTHROUGH = 0x8,
	/* The block only consists of one or two different pixel colors. */
	DETEX_BLOCK_FLAG_MAX_TWO_COLORS = 0x10,
};


struct detexBlockInfo {
	const detexTexture * DETEX_RESTRICT texture;
	int x;
	int y;
	int mode;
	uint32_t flags;
	uint32_t * DETEX_RESTRICT colors;
};

enum detexErrorUnit {
	DETEX_ERROR_UNIT_UINT32,
	DETEX_ERROR_UNIT_UINT64,
	DETEX_ERROR_UNIT_DOUBLE
};

typedef uint32_t (*detexSetPixelsFunc)(const detexBlockInfo *block_info, uint8_t *bitstring);
typedef uint32_t (*detexCalculateErrorFunc)(const detexTexture *texture, int x, int y, uint8_t *pixel_buffer);

struct detexCompressionInfo {
	int nu_modes;
	bool modal_default;
	const int *(*get_modes_func)(detexBlockInfo *block_info);
	detexErrorUnit error_unit;
	void (*seed_func)(const detexBlockInfo *block_info, dstCMWCRNG *rng, uint8_t *bitstring);
	void (*set_mode_func)(uint8_t *bitstring, uint32_t mode, uint32_t flags, uint32_t *colors);
	void (*mutate_func)(const detexBlockInfo *block_info, dstCMWCRNG *rng, int generation, uint8_t *bitstring);
	union {
		uint32_t (*set_pixels_error_uint32_func)(const detexBlockInfo *block_info, uint8_t *bitstring);
		uint64_t (*set_pixels_error_uint64_func)(const detexBlockInfo *block_info, uint8_t *bitstring);
		double (*set_pixels_error_double_func)(const detexBlockInfo *block_info, uint8_t *bitstring);
	};
	union {
		uint32_t (*calculate_error_uint32_func)(const detexTexture *texture, int x, int y, uint8_t *pixel_buffer);
		uint64_t (*calculate_error_uint64_func)(const detexTexture *texture, int x, int y, uint8_t *pixel_buffer);
		double (*calculate_error_double_func)(const detexTexture *texture, int x, int y, uint8_t *pixel_buffer);
	};
};

static DETEX_INLINE_ONLY uint32_t GetPixelErrorRGB8(int r1, int g1, int b1, int r2, int g2, int b2) {
	uint32_t error = (r1 - r2) * (r1 - r2);
	error += (g1 - g2) * (g1 - g2);
	error += (b1 - b2) * (b1 - b2);
	return error;
}

uint32_t detexCalculateErrorRGBA8(const detexTexture *texture, int x, int y, uint8_t *pixel_buffer);

// BC1
void SeedBC1(const detexBlockInfo *info, dstCMWCRNG *rng, uint8_t *bitstring);
void MutateBC1(const detexBlockInfo *info, dstCMWCRNG *rng, int generation, uint8_t *bitstring);
uint32_t SetPixelsBC1(const detexBlockInfo *info, uint8_t *bitstring);

// BC2
void SeedBC2(const detexBlockInfo *info, dstCMWCRNG *rng, uint8_t *bitstring);
void MutateBC2(const detexBlockInfo *info, dstCMWCRNG *rng, int generation, uint8_t *bitstring);
uint32_t SetPixelsBC2(const detexBlockInfo *info, uint8_t *bitstring);

// BC3
void SeedBC3(const detexBlockInfo *info, dstCMWCRNG *rng, uint8_t *bitstring);
void MutateBC3(const detexBlockInfo *info, dstCMWCRNG *rng, int generation, uint8_t *bitstring);
uint32_t SetPixelsBC3(const detexBlockInfo *info, uint8_t *bitstring);

// BC4_UNORM/RGTC1
void SeedRGTC1(const detexBlockInfo *info, dstCMWCRNG *rng, uint8_t *bitstring);
void MutateRGTC1(const detexBlockInfo *info, dstCMWCRNG *rng, int generation, uint8_t *bitstring);
uint32_t SetPixelsRGTC1(const detexBlockInfo *info, uint8_t *bitstring);

// BC4_SNORM/SIGNED_RGTC1
void SeedSignedRGTC1(const detexBlockInfo *info, dstCMWCRNG *rng, uint8_t *bitstring);
void MutateSignedRGTC1(const detexBlockInfo *info, dstCMWCRNG *rng, int generation, uint8_t *bitstring);
uint64_t SetPixelsSignedRGTC1(const detexBlockInfo *info, uint8_t *bitstring);

