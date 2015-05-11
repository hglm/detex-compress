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

#include <stdio.h>
#include "detex.h"

#define DETEX_MAX_LEVELS 32

static bool TextureHasOneBitAlpha(detexTexture *texture) {
	if (!detexFormatHasAlpha(texture->format))
		return true;
	int nu_pixels = texture->width * texture->height;
	bool one_bit_alpha = true;
	if (texture->format == DETEX_PIXEL_FORMAT_RGBA8)
		for (int i = 0; i < nu_pixels; i++) {
			uint32_t pixel = *(uint32_t *)(texture->data + i * 4);
			int a = detexPixel32GetA8(pixel);
			if (a != 0x00 && a != 0xFF)
				one_bit_alpha = false;
		}
	return one_bit_alpha;
}

// Use the averaging method to create an RGBA8/RGBX8 mipmap level from the previous level image (halving
// the dimension) for power-of-two textures. The fields of the destinatino texture must be preinitialized.
static void CreateMipmapWithAveragingRGBA8(detexTexture *source_texture, detexTexture *dest_texture,
bool one_bit_alpha) {
	for (int y = 0; y < source_texture->height; y += 2) {
		int dy = y / 2;
		for (int x = 0; x < source_texture->width; x += 2) {
			int dx = x / 2;
			// Calculate the average values of the pixel block.
			int r = 0;
			int g = 0;
			int b = 0;
			int a = 0;
			uint32_t pixel = *(uint32_t *)(source_texture->data +
				(y * source_texture->width + x) * 4);
			r += detexPixel32GetR8(pixel);
			g += detexPixel32GetG8(pixel);
			b += detexPixel32GetB8(pixel);
			a += detexPixel32GetA8(pixel);
			pixel = *(uint32_t *)(source_texture->data +
				(y * source_texture->width + x + 1) * 4);
			r += detexPixel32GetR8(pixel);
			g += detexPixel32GetG8(pixel);
			b += detexPixel32GetB8(pixel);
			a += detexPixel32GetA8(pixel);
			pixel = *(uint32_t *)(source_texture->data +
				((y + 1) * source_texture->width + x) * 4);
			r += detexPixel32GetR8(pixel);
			g += detexPixel32GetG8(pixel);
			b += detexPixel32GetB8(pixel);
			a += detexPixel32GetA8(pixel);
			pixel = *(uint32_t *)(source_texture->data +
				((y + 1) * source_texture->width + x + 1) * 4);
			r += detexPixel32GetR8(pixel);
			g += detexPixel32GetG8(pixel);
			b += detexPixel32GetB8(pixel);
			a += detexPixel32GetA8(pixel);
			r /= 4;
			g /= 4;
			b /= 4;
			a /= 4;
			if (!detexFormatHasAlpha(source_texture->format))
				a = 0xFF;
			else if (one_bit_alpha) {
				// Avoid smoothing out 1-bit alpha textures. If there are less than two
				// alpha pixels in the source image with alpha 0xFF, then alpha becomes
				// zero, otherwise 0xFF.
				if (a >= 0x80)
					a = 0xFF;
				else
					a = 0;
			}
			pixel = detexPack32RGBA8(r, g, b, a);
			*(uint32_t *)(dest_texture->data + (dy * dest_texture->width + dx) * 4) = pixel;
		}
	}
}

// Use the averaging method to create a 8-bit mipmap level from the previous level image (halving the
// dimension) for power-of-two textures. Used for formats R8 and RG8.
static void CreateMipmapWithAveraging8Bit(detexTexture *source_texture, detexTexture *dest_texture) {
	int nu_components = detexGetNumberOfComponents(source_texture->format);
	int pixel_size = detexGetPixelSize(source_texture->format);
	for (int y = 0; y < source_texture->height; y += 2) {
		int dy = y / 2;
		for (int x = 0; x < source_texture->width; x += 2) {
			int dx = x / 2;
			// For each component, calculate the average values of the pixel block.
			for (int i = 0; i < nu_components; i++) {
				int c = *(uint8_t *)(source_texture->data + (y * source_texture->width + x)
					* pixel_size + i);
				c += *(uint8_t *)(source_texture->data + (y * source_texture->width + x + 1)
					* pixel_size + i);
				c += *(uint8_t *)(source_texture->data + ((y + 1) * source_texture->width + x)
					* pixel_size + i);
				c += *(uint8_t *)(source_texture->data + ((y + 1) * source_texture->width + x + 1)
					* pixel_size + i);
				c /= 4;
				*(uint8_t *)(dest_texture->data + (dy * dest_texture->width + dx)
					* pixel_size + i) = c;
			}
		}
	}
}

// Use the averaging method to create a 8-bit mipmap level from the previous level image (halving the
// dimension) for power-of-two textures. Used for formats SIGNED_R8 and SIGNED_RG8.
static void CreateMipmapWithAveragingSigned8Bit(detexTexture *source_texture, detexTexture *dest_texture) {
	int nu_components = detexGetNumberOfComponents(source_texture->format);
	int pixel_size = detexGetPixelSize(source_texture->format);
	for (int y = 0; y < source_texture->height; y += 2) {
		int dy = y / 2;
		for (int x = 0; x < source_texture->width; x += 2) {
			int dx = x / 2;
			// For each component, calculate the average values of the pixel block.
			for (int i = 0; i < nu_components; i++) {
				int c = *(int8_t *)(source_texture->data + (y * source_texture->width + x)
					* pixel_size + i);
				c += *(int8_t *)(source_texture->data + (y * source_texture->width + x + 1)
					* pixel_size + i);
				c += *(int8_t *)(source_texture->data + ((y + 1) * source_texture->width + x)
					* pixel_size + i);
				c += *(int8_t *)(source_texture->data + ((y + 1) * source_texture->width + x + 1)
					* pixel_size + i);
				c /= 4;
				*(int8_t *)(dest_texture->data + (dy * dest_texture->width + dx)
					* pixel_size + i) = c;
			}
		}
	}
}

// Use the averaging method to create a 16-bit mipmap level from the previous level image (halving the
// dimension) for power-of-two textures.
static void CreateMipmapWithAveraging16Bit(detexTexture *source_texture, detexTexture *dest_texture) {
	int nu_components = detexGetNumberOfComponents(source_texture->format);
	int pixel_size = detexGetPixelSize(source_texture->format);
	for (int y = 0; y < source_texture->height; y += 2) {
		int dy = y / 2;
		for (int x = 0; x < source_texture->width; x += 2) {
			int dx = x / 2;
			for (int i = 0; i < nu_components; i++) {
				int c = *(uint16_t *)(source_texture->data + (y * source_texture->width + x)
					* pixel_size + i * 2);
				c += *(uint16_t *)(source_texture->data + (y * source_texture->width + x + 1)
					* pixel_size + i * 2);
				c += *(uint16_t *)(source_texture->data + ((y + 1) * source_texture->width + x)
					* pixel_size + i * 2);
				c += *(uint16_t *)(source_texture->data + ((y + 1) * source_texture->width + x + 1)
					* pixel_size + i * 2);
				c /= 4;
				*(uint16_t *)(dest_texture->data + (dy * dest_texture->width + dx)
					* pixel_size + i * 2) = c;
			}
		}
	}
}

// Use the averaging method to create a 16-bit mipmap level from the previous level image (halving the
// dimension) for power-of-two textures.
static void CreateMipmapWithAveragingSigned16Bit(detexTexture *source_texture, detexTexture *dest_texture) {
	int nu_components = detexGetNumberOfComponents(source_texture->format);
	int pixel_size = detexGetPixelSize(source_texture->format);
	for (int y = 0; y < source_texture->height; y += 2) {
		int dy = y / 2;
		for (int x = 0; x < source_texture->width; x += 2) {
			int dx = x / 2;
			for (int i = 0; i < nu_components; i++) {
				int c = *(int16_t *)(source_texture->data + (y * source_texture->width + x)
					* pixel_size + i * 2);
				c += *(int16_t *)(source_texture->data + (y * source_texture->width + x + 1)
					* pixel_size + i * 2);
				c += *(int16_t *)(source_texture->data + ((y + 1) * source_texture->width + x)
					* pixel_size + i * 2);
				c += *(int16_t *)(source_texture->data + ((y + 1) * source_texture->width + x + 1)
					* pixel_size + i * 2);
				c /= 4;
				*(int16_t *)(dest_texture->data + (dy * dest_texture->width + dx)
					* pixel_size + i * 2) = c;
			}
		}
	}
}

// Use the averaging method to create a float mipmap level from the previous level image (halving the
// dimension) for power-of-two textures.
static void CreateMipmapWithAveraging32BitFloat(detexTexture *source_texture, detexTexture *dest_texture) {
	int nu_components = detexGetNumberOfComponents(source_texture->format);
	int pixel_size = detexGetPixelSize(source_texture->format);
	for (int y = 0; y < source_texture->height; y += 2) {
		int dy = y / 2;
		for (int x = 0; x < source_texture->width; x += 2) {
			int dx = x / 2;
			for (int i = 0; i < nu_components; i++) {
				float c = *(float *)(source_texture->data + (y * source_texture->width + x)
					* pixel_size + i * 4);
				c += *(float *)(source_texture->data + (y * source_texture->width + x + 1)
					* pixel_size + i * 4);
				c += *(float *)(source_texture->data + ((y + 1) * source_texture->width + x)
					* pixel_size + i * 4);
				c += *(float *)(source_texture->data + ((y + 1) * source_texture->width + x + 1)
					* pixel_size + i * 4);
				c /= 4.0f;
				*(float *)(dest_texture->data + (dy * dest_texture->width + dx)
					* pixel_size + i * 4) = c;
			}
		}
	}
}

#if 0

// Use the averaging method to create a half-float mipmap level from the previous level image (halving the
// dimension) for power-of-two textures.

static void create_half_float_mipmap_with_averaging_divider_2(detexMipmapImage *source_image, detexMipmapImage *dest_image) {
	for (int y = 0; y < source_image->height; y += 2) {
		int dy = y / 2;
		for (int x = 0; x < source_image->width; x += 2) {
			int dx = x / 2;
			// Calculate the average values of the pixel block.
			float c[4];
			c[0] = c[1] = c[2] = c[3] = 0;
			uint64_t pixel = *(uint64_t *)&source_image->pixels[(y * source_image->extended_width + x) * 2];
			uint16_t h[4];
			h[0] = pixel64_get_r16(pixel);
			h[1] = pixel64_get_g16(pixel);
			h[2] = pixel64_get_b16(pixel);
			h[3] = pixel64_get_a16(pixel);
			float f[4];
			halfp2singles(&f[0], &h[0], 4);
			c[0] += f[0];
			c[1] += f[1];
			c[2] += f[2];
			c[3] += f[3];
			pixel = *(uint64_t *)&source_image->pixels[(y * source_image->extended_width + x + 1) * 2];
			h[0] = pixel64_get_r16(pixel);
			h[1] = pixel64_get_g16(pixel);
			h[2] = pixel64_get_b16(pixel);
			h[3] = pixel64_get_a16(pixel);
			halfp2singles(&f[0], &h[0], 4);
			c[0] += f[0];
			c[1] += f[1];
			c[2] += f[2];
			c[3] += f[3];
			pixel = *(uint64_t *)&source_image->pixels[((y + 1) * source_image->extended_width + x) * 2];
			h[0] = pixel64_get_r16(pixel);
			h[1] = pixel64_get_g16(pixel);
			h[2] = pixel64_get_b16(pixel);
			h[3] = pixel64_get_a16(pixel);
			halfp2singles(&f[0], &h[0], 4);
			c[0] += f[0];
			c[1] += f[1];
			c[2] += f[2];
			c[3] += f[3];
			pixel = *(uint64_t *)&source_image->pixels[((y + 1) * source_image->extended_width + x + 1) * 2];
			h[0] = pixel64_get_r16(pixel);
			h[1] = pixel64_get_g16(pixel);
			h[2] = pixel64_get_b16(pixel);
			h[3] = pixel64_get_a16(pixel);
			halfp2singles(&f[0], &h[0], 4);
			c[0] += f[0];
			c[1] += f[1];
			c[2] += f[2];
			c[3] += f[3];
			c[0] /= 4.0;
			c[1] /= 4.0;
			c[2] /= 4.0;
			c[3] /= 4.0;
			if (source_image->alpha_bits == 0) {
				c[3] = 1.0;
			}
			else
			if (source_image->alpha_bits == 1)
				// Avoid smoothing out 1-bit alpha textures. If there are less than two
				// alpha pixels in the source image with alpha 0xFF, then alpha becomes
				// zero, otherwise 0xFF.
				if (c[3] >= 0.5)
					c[3] = 1.0;
				else
					c[3] = 0;
			singles2halfp(&h[0], &c[0], 4);
			pixel = pack_rgba16(h[0], h[1], h[2], h[3]);
			*(uint64_t *)&dest_image->pixels[(dy * dest_image->extended_width + dx) * 2] = pixel;
		}
	}
}

#endif

// Generate a scaled down mipmap image, halving the dimensions.
static void GenerateMipmapLevel(detexTexture *source_texture, bool one_bit_alpha, detexTexture *dest_texture) {
	// Always round down the new dimensions.
	dest_texture->width = source_texture->width / 2;
	dest_texture->height = source_texture->height / 2;
	dest_texture->width_in_blocks = dest_texture->width;
	dest_texture->width_in_blocks = dest_texture->height;
	dest_texture->format = source_texture->format;
	dest_texture->data = (uint8_t *)malloc(dest_texture->width * dest_texture->height *
		detexGetPixelSize(dest_texture->format));
	// Check whether the width and the height are a power of two.
	int count = 0;
	for (int i = 0; i < 30; i++) {
		if (source_texture->width == (1 << i))
			count++;
		if (source_texture->height == (1 << i))
			count++;
	}
	if (count != 2) {
		printf("GenerateMipmapLevel: Cannot handle non-power-of-two texture\n");
		exit(1);
	}
	if (source_texture->format == DETEX_PIXEL_FORMAT_RGBX8 ||
	source_texture->format == DETEX_PIXEL_FORMAT_RGBA8)
		CreateMipmapWithAveragingRGBA8(source_texture, dest_texture, one_bit_alpha);
	else if (source_texture->format == DETEX_PIXEL_FORMAT_R8 ||
	source_texture->format == DETEX_PIXEL_FORMAT_RG8 ||
	source_texture->format == DETEX_PIXEL_FORMAT_RGB8)
		CreateMipmapWithAveraging8Bit(source_texture, dest_texture);
	else if (source_texture->format == DETEX_PIXEL_FORMAT_SIGNED_R8 ||
	source_texture->format == DETEX_PIXEL_FORMAT_SIGNED_RG8)
		CreateMipmapWithAveragingSigned8Bit(source_texture, dest_texture);
	else if (source_texture->format == DETEX_PIXEL_FORMAT_R16 ||
	source_texture->format == DETEX_PIXEL_FORMAT_RG16 ||
	source_texture->format == DETEX_PIXEL_FORMAT_RGB16 ||
	source_texture->format == DETEX_PIXEL_FORMAT_RGBA16)
		CreateMipmapWithAveraging16Bit(source_texture, dest_texture);
	else if (source_texture->format == DETEX_PIXEL_FORMAT_SIGNED_R16 ||
	source_texture->format == DETEX_PIXEL_FORMAT_SIGNED_RG16)
		CreateMipmapWithAveragingSigned16Bit(source_texture, dest_texture);
	else if (source_texture->format == DETEX_PIXEL_FORMAT_FLOAT_R32 ||
	source_texture->format == DETEX_PIXEL_FORMAT_FLOAT_RG32 ||
	source_texture->format == DETEX_PIXEL_FORMAT_FLOAT_RGB32 ||
	source_texture->format == DETEX_PIXEL_FORMAT_FLOAT_RGBA32)
		CreateMipmapWithAveraging32BitFloat(source_texture, dest_texture);
	else {
		printf("GenerateMipmapLevel: Cannot handle format\n");
		exit(1);
	}
}

static int CountMipmapLevels(detexTexture *texture) {
	int i = 1;
	int divider = 2;
	for (;;) {
		int width = texture->width / divider;
		int height = texture->height / divider;
		if (width < 1 || height < 1)
			return i;
		i++;
		divider *= 2;
	}
}

// List of formats for which mipmap generation is supported, with intermediate format used.
static const uint32_t format_table[][2] = {
	{ DETEX_PIXEL_FORMAT_RGBX8, DETEX_PIXEL_FORMAT_RGBX8 },
	{ DETEX_PIXEL_FORMAT_RGBA8, DETEX_PIXEL_FORMAT_RGBA8 },
	{ DETEX_PIXEL_FORMAT_R8, DETEX_PIXEL_FORMAT_R8 },
	{ DETEX_PIXEL_FORMAT_RG8, DETEX_PIXEL_FORMAT_RG8 },
	{ DETEX_PIXEL_FORMAT_RGB8, DETEX_PIXEL_FORMAT_RGB8 },
	{ DETEX_PIXEL_FORMAT_SIGNED_R8, DETEX_PIXEL_FORMAT_SIGNED_R8 },
	{ DETEX_PIXEL_FORMAT_SIGNED_RG8, DETEX_PIXEL_FORMAT_SIGNED_RG8 },
	{ DETEX_PIXEL_FORMAT_R16, DETEX_PIXEL_FORMAT_R16 },
	{ DETEX_PIXEL_FORMAT_RG16, DETEX_PIXEL_FORMAT_RG16 },
	{ DETEX_PIXEL_FORMAT_RGB16, DETEX_PIXEL_FORMAT_RGB16 },
	{ DETEX_PIXEL_FORMAT_RGBA16, DETEX_PIXEL_FORMAT_RGBA16 },
	{ DETEX_PIXEL_FORMAT_SIGNED_R16, DETEX_PIXEL_FORMAT_SIGNED_R16 },
	{ DETEX_PIXEL_FORMAT_SIGNED_RG16, DETEX_PIXEL_FORMAT_SIGNED_RG16 },
	{ DETEX_PIXEL_FORMAT_FLOAT_R32, DETEX_PIXEL_FORMAT_FLOAT_R32 },
	{ DETEX_PIXEL_FORMAT_FLOAT_RG32, DETEX_PIXEL_FORMAT_FLOAT_RG32 },
	{ DETEX_PIXEL_FORMAT_FLOAT_RGB32, DETEX_PIXEL_FORMAT_FLOAT_RGB32 },
	{ DETEX_PIXEL_FORMAT_FLOAT_RGBA32, DETEX_PIXEL_FORMAT_FLOAT_RGBA32 },
	{ DETEX_PIXEL_FORMAT_FLOAT_R16, DETEX_PIXEL_FORMAT_FLOAT_R32 },
	{ DETEX_PIXEL_FORMAT_FLOAT_RG16, DETEX_PIXEL_FORMAT_FLOAT_RG32 },
	{ DETEX_PIXEL_FORMAT_FLOAT_RGB16, DETEX_PIXEL_FORMAT_FLOAT_RGB32 },
	{ DETEX_PIXEL_FORMAT_FLOAT_RGBA16, DETEX_PIXEL_FORMAT_FLOAT_RGBA32 },
};

#define FORMAT_TABLE_SIZE (sizeof(format_table) / sizeof(format_table[0]))

bool detexMipmapGenerationSupported(uint32_t format) {
	for (int i = 0; i < FORMAT_TABLE_SIZE; i++)
		if (format == format_table[i][0])
			return true;
	return false;
}

static void detexCreateMipmaps(detexTexture **textures, int *nu_levels_out) {
	int nu_levels = CountMipmapLevels(textures[0]);
	bool one_bit_alpha = TextureHasOneBitAlpha(textures[0]);
	for (int i = 1; i < nu_levels; i++) {
		textures[i] = (detexTexture *)malloc(sizeof(detexTexture));
		GenerateMipmapLevel(textures[i - 1], one_bit_alpha, textures[i]);
	}
	*nu_levels_out = nu_levels;
}

// Generate mipmaps. The input texture is transferred to the first output texture.
void detexGenerateMipmaps(detexTexture *texture, detexTexture ***mipmap_textures, int *nu_levels_out) {
	detexTexture **textures = (detexTexture **)malloc(sizeof(detexTexture *) * DETEX_MAX_LEVELS);
	int format_table_index;
	for (int i = 0; i < FORMAT_TABLE_SIZE; i++)
		if (texture->format == format_table[i][0]) {
			format_table_index = i;
			break;
		}
	if (format_table[format_table_index][0] == format_table[format_table_index][1])
		textures[0] = texture;
	else {
		// Convert texture to format that can be handled by mipmap generation functions.
		printf("Converting texture.\n");
		textures[0] = (detexTexture *)malloc(sizeof(detexTexture));
		textures[0]->format = format_table[format_table_index][1];
		textures[0]->width = texture->width;
		textures[0]->height = texture->height;
		textures[0]->width_in_blocks = texture->width_in_blocks;
		textures[0]->height_in_blocks = texture->height_in_blocks;
		textures[0]->data = (uint8_t *)malloc(texture->width * texture->height *
			detexGetPixelSize(format_table[format_table_index][1]));
		detexConvertPixels(texture->data, texture->width * texture->height, texture->format,
			textures[0]->data, format_table[format_table_index][1]);
	}

	int nu_levels;
	detexCreateMipmaps(textures, &nu_levels);

	*mipmap_textures = (detexTexture **)malloc(sizeof(detexTexture *) * nu_levels);
	// Transfer the input texture to the first output texture.
	(*mipmap_textures)[0] = texture;
	if (format_table[format_table_index][0] == format_table[format_table_index][1]) {
		for (int i = 1; i < nu_levels; i++)
			(*mipmap_textures)[i] = textures[i];
	}
	else {	
		free(textures[0]->data);
		free(textures[0]);
		for (int i = 1; i < nu_levels; i++) {
			// Convert back to original format.
			uint8_t *data = (uint8_t *)malloc(textures[i]->width * textures[i]->height *
				detexGetPixelSize(format_table[format_table_index][0]));
			detexConvertPixels(textures[i]->data, textures[i]->width * textures[i]->height,
				format_table[format_table_index][1], data,
				format_table[format_table_index][0]);
			free(textures[i]->data);
			textures[i]->data = data;
			textures[i]->format = format_table[format_table_index][0];
			(*mipmap_textures)[i] = textures[i];
		}
	}
	free(textures);
	*nu_levels_out = nu_levels;
}


