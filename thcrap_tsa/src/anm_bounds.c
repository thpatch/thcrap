/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * ANM sprite boundary dumping.
  */

#include <thcrap.h>
#include <png.h>
#include "thcrap_tsa.h"
#include "anm.h"

png_bytep bounds_init(png_image_exp image, thtx_header_t *thtx, const char *fn)
{
	size_t bounds_size;

	if(!image || !thtx) {
		return NULL;
	}

	SAFE_FREE(image->buf);
	png_image_free(&image->img);
	ZeroMemory(&image->img, sizeof(png_image));

	// Still needing this one?
	{
		char *bounds_fn = fn_for_bounds(fn);
		int ret = PathFileExists(bounds_fn);
		SAFE_FREE(bounds_fn);
		if(ret) {
			return NULL;
		}
	}

	image->img.version = PNG_IMAGE_VERSION;
	image->img.width = thtx->w;
	image->img.height = thtx->h;
	image->img.format = PNG_FORMAT_RGBA;

	bounds_size = PNG_IMAGE_SIZE(image->img);

	image->buf = malloc(bounds_size);
	if(image->buf) {
		ZeroMemory(image->buf, bounds_size);
	}
	return image->buf;
}

png_bytep bounds_resize(png_image_exp image, const size_t new_w, const size_t new_h)
{
	size_t prev_w;
	size_t prev_h;
	size_t prev_stride;
	size_t prev_size;
	size_t new_size;
	size_t new_stride;

	if(!image || !image->buf) {
		return NULL;
	}

	prev_w = image->img.width;
	prev_h = image->img.height;

	if(prev_w <= new_w && prev_h <= new_h) {
		return image->buf;
	}

	prev_stride = PNG_IMAGE_ROW_STRIDE(image->img);
	prev_size = PNG_IMAGE_SIZE(image->img);

	image->img.width = new_w;
	image->img.height = new_h;
	new_stride = PNG_IMAGE_ROW_STRIDE(image->img);
	new_size = PNG_IMAGE_SIZE(image->img);

	image->buf = realloc(image->buf, new_size);
	if(image->buf) {
		ZeroMemory(image->buf + prev_size, new_size - prev_size);

		// If the width has changed, we need to move all rows in the image,
		// going from bottom to top.
		// (Luckily, this is the only case we have to implement.)
		if(new_w > prev_w && new_h >= prev_h) {
			png_bytep in = image->buf + (prev_stride * (prev_h - 1));
			png_bytep out = image->buf + (new_stride * (prev_h - 1));
			size_t i;
			for(i = 0; i < prev_h; i++, out -= new_stride, in -= prev_stride) {
				memmove(out, in, prev_stride);
				ZeroMemory(out + prev_stride, new_stride - prev_stride);
			}
		}
	}
	return image->buf;
}

char* fn_for_bounds(const char *fn)
{
	const char *dir = json_object_get_string(runconfig_get(), "dat_dump");
	const char *prefix = "bounds-";
	size_t ret_len;
	char *ret = NULL;
	char *game_fn = fn_for_game(fn);
	char *p;

	if(!game_fn) {
		return ret;
	}
	if(!dir) {
		dir = "dat";
	}

	ret_len = strlen(dir) + 1 + strlen(prefix) + strlen(game_fn) + 1;
	ret = malloc(ret_len);
	// Start replacements after the directory
	p = ret + strlen(dir);

	sprintf(ret, "%s/%s%s", dir, prefix, game_fn);

	while(*p++) {
		if(*p == '/' || *p == '\\') {
			*p = '-';
		}
	}
	SAFE_FREE(game_fn);
	return ret;
}

int bounds_draw_line(
	png_image_exp image,
	const size_t x, const size_t y,
	const size_t pixeldist, const size_t len, png_byte val
)
{
	png_byte bpp;
	size_t stride;
	size_t offset;
	size_t i;
	png_bytep out;

	if(!image || !image->buf || !len || x >= image->img.width || y >= image->img.height) {
		return -1;
	}

	bpp = PNG_IMAGE_PIXEL_SIZE(image->img.format);
	stride = bpp * image->img.width;
	offset = (y * stride) + (x * bpp);

	for(
		(i = 0), (out = image->buf + offset);
		(i < len) && (out < image->buf + PNG_IMAGE_SIZE(image->img));
		(i++), (out += pixeldist * bpp)
	) {
		out[0] = out[bpp - 1] = val;
	}
	return 0;
}

int bounds_draw_hline(
	png_image_exp image, const size_t x, const size_t y, const size_t len, png_byte val
)
{
	return bounds_draw_line(image, x, y, 1, len, val);
}

int bounds_draw_vline(
	png_image_exp image, const size_t x, const size_t y, const size_t len, png_byte val
)
{
	if(!image) {
		return -1;
	}
	return bounds_draw_line(image, x, y, image->img.width, len, val);
}

int bounds_draw_rect(png_image_exp image, const size_t thtx_x, const size_t thtx_y, sprite_t *spr)
{
	if(!spr) {
		return -1;
	} else {
		// MSVC doesn't optimize this and would reconvert these values to integers
		// every time they are used below.
		size_t spr_x = (size_t)spr->x;
		size_t spr_y = (size_t)spr->y;
		size_t spr_w = (size_t)spr->w;
		size_t spr_h = (size_t)spr->h;

		size_t real_x = thtx_x + spr_x;
		size_t real_y = thtx_y + spr_y;

		bounds_draw_hline(image, real_x, real_y, spr_w, 0xFF);
		bounds_draw_hline(image, real_x, real_y + (spr_h - 1), spr_w, 0xFF);

		bounds_draw_vline(image, real_x, real_y, spr_h, 0xFF);
		bounds_draw_vline(image, real_x + (spr_w - 1), real_y, spr_h, 0xFF);
	}
	return 0;
}

int bounds_store(const char *fn, png_image_exp image)
{
	char *bounds_fn;
	if(!fn || !image || !image->buf) {
		return -1;
	}
	bounds_fn = fn_for_bounds(fn);
	if(!bounds_fn) {
		return 1;
	}
	png_image_write_to_file(&image->img, bounds_fn, 0, image->buf, 0, NULL);
	SAFE_FREE(bounds_fn);
	return 0;
}
