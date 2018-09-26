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
#include "png_ex.h"
#include "thcrap_tsa.h"
#include "anm.hpp"

void bounds_init(png_image_ex &image, const thtx_header_t *thtx, const char *fn)
{
	// The caller expects the image to be cleared in any case
	png_image_clear(image);
	if(thtx && fn) {
		// Still needing this one?
		char *bounds_fn = fn_for_bounds(fn);
		int ret = PathFileExists(bounds_fn);
		SAFE_FREE(bounds_fn);
		if(!ret) {
			png_image_new(image, thtx->w, thtx->h, PNG_FORMAT_RGBA);
		}
	}
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
	ret = (char *)malloc(ret_len);
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
	png_image_ex &image,
	const size_t x, const size_t y,
	const size_t pixeldist, const size_t len, png_byte val
)
{
	png_byte bpp;
	size_t stride;
	size_t offset;
	size_t i;
	png_bytep out;

	if(!image.buf || !len || x >= image.img.width || y >= image.img.height) {
		return -1;
	}

	bpp = PNG_IMAGE_PIXEL_SIZE(image.img.format);
	stride = bpp * image.img.width;
	offset = (y * stride) + (x * bpp);

	for(
		(i = 0), (out = image.buf + offset);
		(i < len) && (out < image.buf + PNG_IMAGE_SIZE(image.img));
		(i++), (out += pixeldist * bpp)
	) {
		out[0] = out[bpp - 1] = val;
	}
	return 0;
}

int bounds_draw_hline(
	png_image_ex &image, const size_t x, const size_t y, const size_t len, png_byte val
)
{
	return bounds_draw_line(image, x, y, 1, len, val);
}

int bounds_draw_vline(
	png_image_ex &image, const size_t x, const size_t y, const size_t len, png_byte val
)
{
	return bounds_draw_line(image, x, y, image.img.width, len, val);
}

int bounds_draw_rect(
	png_image_ex &image, const size_t thtx_x, const size_t thtx_y, const sprite_local_t *spr
)
{
	size_t real_x = thtx_x + spr->x;
	size_t real_y = thtx_y + spr->y;

	bounds_draw_hline(image, real_x, real_y, spr->w, 0xFF);
	bounds_draw_hline(image, real_x, real_y + (spr->h - 1), spr->w, 0xFF);

	bounds_draw_vline(image, real_x, real_y, spr->h, 0xFF);
	bounds_draw_vline(image, real_x + (spr->w - 1), real_y, spr->h, 0xFF);
	return 0;
}

int bounds_store(const char *fn, png_image_ex &image)
{
	char *bounds_fn;
	if(!fn || !image.buf) {
		return -1;
	}
	bounds_fn = fn_for_bounds(fn);
	if(!bounds_fn) {
		return 1;
	}
	png_image_store(bounds_fn, image);
	SAFE_FREE(bounds_fn);
	return 0;
}
