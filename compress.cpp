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

#include <sched.h>
#include <dstRandom.h>
#include "detex.h"
#include "compress.h"
#include "compress-block.h"

// #define VERBOSE

static DETEX_INLINE_ONLY void AddErrorPixelXYRGBX8(const uint8_t * DETEX_RESTRICT pix1,
const uint8_t * DETEX_RESTRICT pix2, int pix2_stride, int dx, int dy, uint32_t & DETEX_RESTRICT error) {
	uint32_t pixel1 = *(uint32_t *)(pix1 + (dy * 4 + dx) * 4);
	int r1 = detexPixel32GetR8(pixel1);
	int g1 = detexPixel32GetG8(pixel1);
	int b1 = detexPixel32GetB8(pixel1);
	uint32_t pixel2 = *(uint32_t *)(pix2 + dy * pix2_stride + dx * 4);
	int r2 = detexPixel32GetR8(pixel2);
	int g2 = detexPixel32GetG8(pixel2);
	int b2 = detexPixel32GetB8(pixel2);
	error += GetPixelErrorRGB8(r1, g1, b1, r2, g2, b2);
}

static uint32_t detexCalculateErrorRGBX8(const detexTexture * DETEX_RESTRICT texture, int x, int y,
uint8_t * DETEX_RESTRICT pixel_buffer) {
	uint8_t *pix1 = pixel_buffer;
	uint8_t *pix2 = texture->data + (y * texture->width + x) * 4;
	int pix2_stride = texture->width * 4;
	uint32_t error = 0;
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 0, 0, error);
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 1, 0, error);
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 2, 0, error);
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 3, 0, error);
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 0, 1, error);
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 1, 1, error);
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 2, 1, error);
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 3, 1, error);
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 0, 2, error);
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 1, 2, error);
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 2, 2, error);
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 3, 2, error);
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 0, 3, error);
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 1, 3, error);
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 2, 3, error);
	AddErrorPixelXYRGBX8(pix1, pix2, pix2_stride, 3, 3, error);
	return error;
}

static DETEX_INLINE_ONLY void AddErrorPixelXYRGBA8(const uint8_t * DETEX_RESTRICT pix1,
const uint8_t * DETEX_RESTRICT pix2, int pix2_stride, int dx, int dy, uint32_t & DETEX_RESTRICT error) {
	uint32_t pixel1 = *(uint32_t *)(pix1 + (dy * 4 + dx) * 4);
	uint32_t pixel2 = *(uint32_t *)(pix2 + dy * pix2_stride + dx * 4);
	int a1 = detexPixel32GetA8(pixel1);
	int a2 = detexPixel32GetA8(pixel2);
	if ((a1 | a2) != 0) {
		error += (a1 - a2) * (a1 - a2);
		int r1 = detexPixel32GetR8(pixel1);
		int g1 = detexPixel32GetG8(pixel1);
		int b1 = detexPixel32GetB8(pixel1);
		int r2 = detexPixel32GetR8(pixel2);
		int g2 = detexPixel32GetG8(pixel2);
		int b2 = detexPixel32GetB8(pixel2);
		error += GetPixelErrorRGB8(r1, g1, b1, r2, g2, b2);
	}
}

uint32_t detexCalculateErrorRGBA8(const detexTexture * DETEX_RESTRICT texture, int x, int y,
uint8_t * DETEX_RESTRICT pixel_buffer) {
	uint8_t *pix1 = pixel_buffer;
	uint8_t *pix2 = texture->data + (y * texture->width + x) * 4;
	int pix2_stride = texture->width * 4;
	uint32_t error = 0;
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 0, 0, error);
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 1, 0, error);
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 2, 0, error);
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 3, 0, error);
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 0, 1, error);
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 1, 1, error);
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 2, 1, error);
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 3, 1, error);
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 0, 2, error);
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 1, 2, error);
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 2, 2, error);
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 3, 2, error);
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 0, 3, error);
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 1, 3, error);
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 2, 3, error);
	AddErrorPixelXYRGBA8(pix1, pix2, pix2_stride, 3, 3, error);
	return error;
}

static DETEX_INLINE_ONLY void AddErrorPixelXYR8(const uint8_t * DETEX_RESTRICT pix1,
const uint8_t * DETEX_RESTRICT pix2, int pix2_stride, int dx, int dy, uint32_t & DETEX_RESTRICT error) {
	uint32_t r1 = *(pix1 + (dy * 4 + dx));
	uint32_t r2 = *(pix2 + dy * pix2_stride + dx);
	error += (r1 - r2) * (r1 - r2);
}

uint32_t detexCalculateErrorR8(const detexTexture * DETEX_RESTRICT texture, int x, int y,
uint8_t * DETEX_RESTRICT pixel_buffer) {
	uint8_t *pix1 = pixel_buffer;
	uint8_t *pix2 = texture->data + (y * texture->width + x);
	int pix2_stride = texture->width;
	uint32_t error = 0;
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 0, 0, error);
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 1, 0, error);
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 2, 0, error);
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 3, 0, error);
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 0, 1, error);
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 1, 1, error);
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 2, 1, error);
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 3, 1, error);
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 0, 2, error);
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 1, 2, error);
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 2, 2, error);
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 3, 2, error);
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 0, 3, error);
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 1, 3, error);
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 2, 3, error);
	AddErrorPixelXYR8(pix1, pix2, pix2_stride, 3, 3, error);
	return error;
}

static DETEX_INLINE_ONLY void AddErrorPixelXYSignedR16(const uint8_t * DETEX_RESTRICT pix1,
const uint8_t * DETEX_RESTRICT pix2, int pix2_stride, int dx, int dy, uint64_t & DETEX_RESTRICT error) {
	int r1 = *(int16_t *)(pix1 + (dy * 4 + dx) * 2);
	int r2 = *(int16_t *)(pix2 + dy * pix2_stride + dx * 2);
	error += (r1 - r2) * (r1 - r2);
}

uint64_t detexCalculateErrorSignedR16(const detexTexture * DETEX_RESTRICT texture, int x, int y,
uint8_t * DETEX_RESTRICT pixel_buffer) {
	uint8_t *pix1 = pixel_buffer;
	uint8_t *pix2 = texture->data + (y * texture->width + x) * 2;
	int pix2_stride = texture->width * 2;
	uint64_t error = 0;
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 0, 0, error);
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 1, 0, error);
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 2, 0, error);
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 3, 0, error);
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 0, 1, error);
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 1, 1, error);
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 2, 1, error);
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 3, 1, error);
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 0, 2, error);
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 1, 2, error);
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 2, 2, error);
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 3, 2, error);
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 0, 3, error);
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 1, 3, error);
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 2, 3, error);
	AddErrorPixelXYSignedR16(pix1, pix2, pix2_stride, 3, 3, error);
	return error;
}

static DETEX_INLINE_ONLY void AddErrorPixelXYRG8(const uint8_t * DETEX_RESTRICT pix1,
const uint8_t * DETEX_RESTRICT pix2, int pix2_stride, int dx, int dy, uint32_t & DETEX_RESTRICT error) {
	uint32_t r1 = *(pix1 + (dy * 4 + dx) * 2);
	uint32_t g1 = *(pix1 + (dy * 4 + dx) * 2 + 1);
	uint32_t r2 = *(pix2 + dy * pix2_stride + dx * 2);
	uint32_t g2 = *(pix2 + dy * pix2_stride + dx * 2 + 1);
	error += (r1 - r2) * (r1 - r2);
	error += (g1 - g2) * (g1 - g2);
}

uint32_t detexCalculateErrorRG8(const detexTexture * DETEX_RESTRICT texture, int x, int y,
uint8_t * DETEX_RESTRICT pixel_buffer) {
	uint8_t *pix1 = pixel_buffer;
	uint8_t *pix2 = texture->data + (y * texture->width + x) * 2;
	int pix2_stride = texture->width * 2;
	uint32_t error = 0;
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 0, 0, error);
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 1, 0, error);
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 2, 0, error);
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 3, 0, error);
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 0, 1, error);
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 1, 1, error);
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 2, 1, error);
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 3, 1, error);
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 0, 2, error);
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 1, 2, error);
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 2, 2, error);
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 3, 2, error);
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 0, 3, error);
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 1, 3, error);
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 2, 3, error);
	AddErrorPixelXYRG8(pix1, pix2, pix2_stride, 3, 3, error);
	return error;
}

static DETEX_INLINE_ONLY void AddErrorPixelXYSignedRG16(const uint8_t * DETEX_RESTRICT pix1,
const uint8_t * DETEX_RESTRICT pix2, int pix2_stride, int dx, int dy, uint64_t & DETEX_RESTRICT error) {
	int r1 = *(int16_t *)(pix1 + (dy * 4 + dx) * 4);
	int r2 = *(int16_t *)(pix2 + dy * pix2_stride + dx * 4);
	int g1 = *(int16_t *)(pix1 + (dy * 4 + dx) * 4 + 2);
	int g2 = *(int16_t *)(pix2 + dy * pix2_stride + dx * 4 + 2);
	error += (int64_t)(r1 - r2) * (r1 - r2);
	error += (int64_t)(g2 - g2) * (g1 - g2);
}

uint64_t detexCalculateErrorSignedRG16(const detexTexture * DETEX_RESTRICT texture, int x, int y,
uint8_t * DETEX_RESTRICT pixel_buffer) {
	uint8_t *pix1 = pixel_buffer;
	uint8_t *pix2 = texture->data + (y * texture->width + x) * 4;
	int pix2_stride = texture->width * 4;
	uint64_t error = 0;
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 0, 0, error);
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 1, 0, error);
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 2, 0, error);
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 3, 0, error);
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 0, 1, error);
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 1, 1, error);
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 2, 1, error);
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 3, 1, error);
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 0, 2, error);
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 1, 2, error);
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 2, 2, error);
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 3, 2, error);
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 0, 3, error);
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 1, 3, error);
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 2, 3, error);
	AddErrorPixelXYSignedRG16(pix1, pix2, pix2_stride, 3, 3, error);
	return error;
}

static const uint32_t supported_formats[] = {
	DETEX_TEXTURE_FORMAT_BC1,
	DETEX_TEXTURE_FORMAT_BC2,
	DETEX_TEXTURE_FORMAT_BC3,
	DETEX_TEXTURE_FORMAT_RGTC1,
	DETEX_TEXTURE_FORMAT_SIGNED_RGTC1,
	DETEX_TEXTURE_FORMAT_RGTC2,
	DETEX_TEXTURE_FORMAT_SIGNED_RGTC2,
};

#define NU_SUPPORTED_FORMATS (sizeof(supported_formats) / sizeof(supported_formats[0]))

bool detexCompressionSupported(uint32_t format) {
	for (int i = 0; i < NU_SUPPORTED_FORMATS; i++)
		if (format == supported_formats[i])
			return true;
	return false;
}

static const detexCompressionInfo compression_info[] = {
	// BC1
	{ 2, true, DETEX_ERROR_UNIT_UINT32, SeedBC1, detexGetModeBC1, detexSetModeBC1,
	MutateBC1, SetPixelsBC1, detexCalculateErrorRGBX8 },
	// BC1A
	{ 2, true, DETEX_ERROR_UNIT_UINT32, NULL, NULL, NULL, NULL, NULL },
	// BC2
	// Use modal configuration with just one mode. This ensures the color definitions
	// comply to mode 0, as required for BC2.
	{ 1, true, DETEX_ERROR_UNIT_UINT32, SeedBC2, NULL, NULL,
	MutateBC2, SetPixelsBC2, detexCalculateErrorRGBA8 },
	// BC3
	{ 2, true, DETEX_ERROR_UNIT_UINT32, SeedBC3, NULL, NULL,
	MutateBC3, SetPixelsBC3, detexCalculateErrorRGBA8 },
	// RGTC1
	{ 2, true, DETEX_ERROR_UNIT_UINT32, SeedRGTC1, NULL, NULL,
	MutateRGTC1, SetPixelsRGTC1, detexCalculateErrorR8 },
	// SIGNED_RGTC1
	{ 2, true, DETEX_ERROR_UNIT_UINT64, SeedSignedRGTC1, NULL, NULL,
	MutateSignedRGTC1, (detexSetPixelsFunc)SetPixelsSignedRGTC1,
	(detexCalculateErrorFunc)detexCalculateErrorSignedR16 },
	// RGTC2
	{ 4, true, DETEX_ERROR_UNIT_UINT32, NULL, NULL, NULL,
	NULL, NULL, detexCalculateErrorRG8 },
	// SIGNED_RGTC2
	{ 4, true, DETEX_ERROR_UNIT_UINT64, NULL, NULL, NULL,
	NULL, NULL, (detexCalculateErrorFunc)detexCalculateErrorSignedRG16 },
};

static double detexCompressBlock(const detexCompressionInfo * DETEX_RESTRICT info,
const detexBlockInfo * DETEX_RESTRICT block_info, dstCMWCRNG *rng,
uint8_t * DETEX_RESTRICT bitstring_out, uint32_t output_format) {
	uint8_t bitstring[16];
//	uint8_t pixel_buffer[DETEX_MAX_BLOCK_SIZE];
	int compressed_block_size = detexGetCompressedBlockSize(output_format);
	uint32_t best_error_uint32 = UINT_MAX;
	uint64_t best_error_uint64 = UINT64_MAX;
	double best_error_double = DBL_MAX;
	int last_improvement_generation = -1;
	for (int generation = 0; generation < 2048 || last_improvement_generation > generation - 384;) {
		if (generation < 256) {
			// For the first 256 iterations, use the seeding function.
			info->seed_func(block_info, rng, bitstring);
		}
		else {
			// From iteration 256, use mutation.
			memcpy(bitstring, bitstring_out, compressed_block_size);
			info->mutate_func(block_info, rng, generation,bitstring);
		}
		uint32_t error_uint32;
		uint64_t error_uint64;
		double error_double;
		bool is_better;
		if (info->error_unit == DETEX_ERROR_UNIT_UINT32) {
			error_uint32 = info->set_pixels_error_uint32_func(block_info, bitstring);
			is_better = (error_uint32 < best_error_uint32);
			if (is_better)
				best_error_uint32 = error_uint32;
#ifdef VERBOSE
			if ((generation & 127) == 0)
				printf("Gen %d: RMSE = %.3f\n", generation,
					sqrt((double)best_error_uint32 / 16.0d));
#endif
		}
		else if (info->error_unit == DETEX_ERROR_UNIT_UINT64) {
			error_uint64 = info->set_pixels_error_uint64_func(block_info, bitstring);
			is_better = (error_uint64 < best_error_uint64);
			if (is_better)
				best_error_uint64 = error_uint64;
		}
		else if (info->error_unit == DETEX_ERROR_UNIT_DOUBLE) {
			error_double = info->set_pixels_error_double_func(block_info, bitstring);
			is_better = (error_double < best_error_double);
			if (is_better)
				best_error_double = error_double;
		}
		if (is_better) {
			memcpy(bitstring_out, bitstring, compressed_block_size);
			last_improvement_generation = generation;
		}
		generation++;
		if (info->error_unit == DETEX_ERROR_UNIT_UINT32 && error_uint32 == 0)
			break;
	}
	double rmse = sqrt((double)best_error_uint32 / 16.0d);
#ifdef VERBOSE
	printf("Block RMSE (mode %d): %.3f\n", block_info->mode, rmse);
#endif
	return rmse;
}

struct ThreadData {
	const detexTexture *texture;
	uint8_t *pixel_buffer;
	uint32_t output_format;
	int x_start;
	int y_start;
	int x_end;
	int y_end;
	int nu_tries;
	bool modal;
	dstCMWCRNG *rng;
};

static void *CompressBlocksThread(void *_thread_data) {
	ThreadData *thread_data = (ThreadData *)_thread_data;
	const detexTexture *texture = thread_data->texture;
	uint8_t *pixel_buffer = thread_data->pixel_buffer;
	int compressed_format_index = detexGetCompressedFormat(thread_data->output_format);
	int block_size = detexGetCompressedBlockSize(thread_data->output_format);
	for (int y = thread_data->y_start; y < thread_data->y_end; y += 4)
		for (int x = 0; x < thread_data->x_end; x += 4) {
			detexBlockInfo block_info;
			block_info.texture = texture;
			block_info.x = x;
			block_info.y = y;
			block_info.flags = 0;
			block_info.colors = NULL;
			// Calculate the block index.
			int i = (y / 4) * (texture->width / 4) + x / 4;
			double best_rmse = DBL_MAX;
			for (int j = 0; j < thread_data->nu_tries; j++) {
				uint8_t bitstring[16];
				if (thread_data->modal)
					// Compress the block using each mode.
					for (int mode = 0; mode < compression_info[compressed_format_index
					- 1].nu_modes; mode++) {
						block_info.mode = mode;
						double rmse = detexCompressBlock(
							&compression_info[compressed_format_index - 1],
							&block_info, thread_data->rng,
							bitstring, thread_data->output_format);
						if (rmse < best_rmse) {
							best_rmse = rmse;
							memcpy(&pixel_buffer[i * block_size], bitstring, block_size);
							if (rmse == 0.0d)
								break;
						}
					}
				else {
					block_info.mode = -1;
					double rmse = detexCompressBlock(
						&compression_info[compressed_format_index - 1],
						&block_info, thread_data->rng,
						bitstring, thread_data->output_format);
					if (rmse < best_rmse) {
						best_rmse = rmse;
						memcpy(&pixel_buffer[i * block_size], bitstring, block_size);
					}
				}
				if (best_rmse == 0.0d)
					break;
			}
		}
	return NULL;
}

bool detexCompressTexture(int nu_tries, bool modal, int max_threads, const detexTexture * DETEX_RESTRICT texture,
uint8_t * DETEX_RESTRICT pixel_buffer, uint32_t output_format) {
	// Special handling for compressed texture formats that can be composited from compression
	// of other formats.
	if (output_format == DETEX_TEXTURE_FORMAT_RGTC2) {
		// The input texture is in format DETEX_PIXEL_FORMAT_RG8.
		// Create a temporary texture with just the red components.
		int nu_pixels = texture->width * texture->height;
		detexTexture temp_texture = *texture;
		temp_texture.format = DETEX_PIXEL_FORMAT_R8;
		temp_texture.data = (uint8_t *)malloc(nu_pixels);
		for (int i = 0; i < nu_pixels; i++)
			temp_texture.data[i] = texture->data[i * 2];
		int nu_blocks = texture->width * texture->height / 16;
		uint8_t *temp_pixel_buffer = (uint8_t *)malloc(nu_blocks * 8);
		// Compress the red components.
		detexCompressTexture(nu_tries, modal, max_threads, &temp_texture, temp_pixel_buffer,
			DETEX_TEXTURE_FORMAT_RGTC1);
		for (int i = 0; i < nu_blocks; i++)
			*(uint64_t *)(pixel_buffer + i * 16) = *(uint64_t *)(temp_pixel_buffer + i * 8);
		// Create a temporary texture with just the green components.
		for (int i = 0; i < nu_pixels; i++)
			temp_texture.data[i] = texture->data[i * 2 + 1];
		// Compress the green components.
		detexCompressTexture(nu_tries, modal, max_threads, &temp_texture, temp_pixel_buffer,
			DETEX_TEXTURE_FORMAT_RGTC1);
		free(temp_texture.data);
		for (int i = 0; i < nu_blocks; i++)
			*(uint64_t *)(pixel_buffer + i * 16 + 8) = *(uint64_t *)(temp_pixel_buffer + i * 8);
		free(temp_pixel_buffer);
		return true;
	}
	else if (output_format == DETEX_TEXTURE_FORMAT_SIGNED_RGTC2) {
		// The input texture is in format DETEX_PIXEL_FORMAT_SIGNED_RG16.
		// Create a temporary texture with just the red components.
		int nu_pixels = texture->width * texture->height;
		detexTexture temp_texture = *texture;
		temp_texture.format = DETEX_PIXEL_FORMAT_SIGNED_R16;
		temp_texture.data = (uint8_t *)malloc(nu_pixels * 2);
		for (int i = 0; i < nu_pixels; i++)
			*(int16_t *)(temp_texture.data + i * 2) = *(int16_t *)(texture->data + i * 4);
		int nu_blocks = texture->width * texture->height / 16;
		uint8_t *temp_pixel_buffer = (uint8_t *)malloc(nu_blocks * 8);
		// Compress the red components.
		detexCompressTexture(nu_tries, modal, max_threads, &temp_texture, temp_pixel_buffer,
			DETEX_TEXTURE_FORMAT_SIGNED_RGTC1);
		for (int i = 0; i < nu_blocks; i++)
			*(uint64_t *)(pixel_buffer + i * 16) = *(uint64_t *)(temp_pixel_buffer + i * 8);
		// Create a temporary texture with just the green components.
		for (int i = 0; i < nu_pixels; i++)
			*(int16_t *)(temp_texture.data + i * 2) = *(int16_t *)(texture->data + i * 4 + 2);
		// Compress the green components.
		detexCompressTexture(nu_tries, modal, max_threads, &temp_texture, temp_pixel_buffer,
			DETEX_TEXTURE_FORMAT_SIGNED_RGTC1);
		free(temp_texture.data);
		for (int i = 0; i < nu_blocks; i++)
			*(uint64_t *)(pixel_buffer + i * 16 + 8) = *(uint64_t *)(temp_pixel_buffer + i * 8);
		free(temp_pixel_buffer);
		return true;
	}
	// Calculate the number of blocks.
	int nu_blocks = (texture->height / 4) * (texture->width / 4);
	int nu_threads;
	if (max_threads > 0)
		nu_threads = max_threads;
	else
		// A number of threads higher than the number of CPU cores helps performance
		// on PC-class devices.
		nu_threads = sysconf(_SC_NPROCESSORS_CONF) * 2;
	int nu_blocks_per_thread = nu_blocks / nu_threads;
	if (nu_blocks_per_thread < 32) {
		nu_threads = nu_blocks / 32;
		if (nu_threads == 0)
			nu_threads = 1;
	}
	pthread_t *thread = (pthread_t *)malloc(sizeof(pthread_t) * nu_threads);
	ThreadData *thread_data = (ThreadData *)malloc(sizeof(ThreadData) * nu_threads);
	for (int i = 0; i < nu_threads; i++) {
		thread_data[i].texture = texture;
		thread_data[i].pixel_buffer = pixel_buffer;
		thread_data[i].output_format = output_format;
		thread_data[i].x_start = 0;
		thread_data[i].y_start = i * texture->height / nu_threads;
		thread_data[i].x_end = texture->width;
		thread_data[i].y_end = (i + 1) * texture->height / nu_threads;
		thread_data[i].nu_tries = nu_tries;
		thread_data[i].modal = modal;
		thread_data[i].rng = new dstCMWCRNG;
		if (i < nu_threads - 1)
			pthread_create(&thread[i], NULL, CompressBlocksThread, &thread_data[i]);
		else
			CompressBlocksThread(&thread_data[i]);			
	}
	for (int i = 0; i < nu_threads - 1; i++)
		pthread_join(thread[i], NULL);
	for (int i = 0; i < nu_threads; i++)
		delete thread_data[i].rng;
	free(thread_data);
	free(thread);
	return true;
}

// Compare and return RMSE.
double detexCompareTextures(const detexTexture * DETEX_RESTRICT input_texture,
detexTexture * DETEX_RESTRICT compressed_texture, double *average_rmse, double *rmse_sd) {
	uint8_t pixel_buffer[DETEX_MAX_BLOCK_SIZE];
	int compressed_format_index = detexGetCompressedFormat(compressed_texture->format);
	const detexCompressionInfo *info = &compression_info[compressed_format_index - 1];
	int block_size = detexGetCompressedBlockSize(compressed_texture->format);
	double total_error = 0;
	int nu_blocks = compressed_texture->width_in_blocks * compressed_texture->height_in_blocks;
	double *block_rmse = (double *)malloc(sizeof(double) * nu_blocks);
	for (int y = 0; y < compressed_texture->height; y += 4)
		for (int x = 0; x < compressed_texture->width; x += 4) {
			// Calculate the block index.
			int i = (y / 4) * (compressed_texture->width / 4) + x / 4;
			// Decompress block.
			bool r = detexDecompressBlock(&compressed_texture->data[i * block_size],
				compressed_texture->format, DETEX_MODE_MASK_ALL,
				DETEX_DECOMPRESS_FLAG_ENCODE, pixel_buffer,
				detexGetPixelFormat(compressed_texture->format));
			if (!r) {
				printf("Error during decompression for final image comparison\n");
				exit(1);
			}
			if (info->error_unit == DETEX_ERROR_UNIT_UINT32) {
				uint32_t error = info->calculate_error_uint32_func(input_texture, x, y,
					pixel_buffer);
				total_error += error;
				block_rmse[i] = sqrt(error / 16);
			}
			else if (info->error_unit == DETEX_ERROR_UNIT_UINT64) {
				uint64_t error = info->calculate_error_uint64_func(input_texture, x, y,
					pixel_buffer);
				total_error += error;
				block_rmse[i] = sqrt(error / 16);
			}
			else if (info->error_unit == DETEX_ERROR_UNIT_DOUBLE) {
				double error = info->calculate_error_double_func(input_texture, x, y,
					pixel_buffer);
				total_error += error;
				block_rmse[i] = sqrt(error / 16);
			}
		}
	int nu_pixels = compressed_texture->width * compressed_texture->height;
	double rmse = sqrt(total_error / nu_pixels);
	if (average_rmse != NULL || rmse_sd != NULL) {
		double average = 0.0d;
		for (int i = 0; i < nu_blocks; i++)
			average += block_rmse[i];
		average /= nu_blocks;
		if (average_rmse != NULL)
			*average_rmse = average;
		if (rmse_sd != NULL) {
			double variance = 0.0d;
			for (int i = 0; i < nu_blocks; i++)
				variance += (block_rmse[i] - average) * (block_rmse[i] - average);
			variance /= nu_blocks;
			if (rmse_sd != NULL)
				*rmse_sd = sqrt(variance);
		}
	}
	free(block_rmse);
	return rmse;
}

int detexGetNumberOfModes(uint32_t format) {
	int compressed_format_index = detexGetCompressedFormat(format);
	return compression_info[compressed_format_index - 1].nu_modes;
}

bool detexGetModalDefault(uint32_t format) {
	int compressed_format_index = detexGetCompressedFormat(format);
	return compression_info[compressed_format_index - 1].modal_default;
}

