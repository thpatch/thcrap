/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * On-the-fly ANM patcher.
  *
  * Portions adapted from xarnonymous' Touhou Toolkit
  * http://code.google.com/p/thtk/
  */

#include <thcrap.h>
#include <png.h>
#include "thcrap_tsa.h"
#include "anm.h"

/// JSON-based structure data access
/// --------------------------------
int struct_get(void *dest, size_t dest_size, void *src, json_t *spec)
{
	if(!dest || !dest_size || !src || !spec) {
		return -1;
	}
	{
		size_t offset = json_object_get_hex(spec, "offset");
		size_t size = json_object_get_hex(spec, "size");
		// Default to architecture word size
		if(!size) {
			size = sizeof(size_t);
		};
		if(size > dest_size) {
			return -2;
		}
		ZeroMemory(dest, dest_size);
		memcpy(dest, (char*)src + offset, size);
	}
	return 0;
}

#define STRUCT_GET(type, val, src, spec_obj) \
	struct_get(&(val), sizeof(type), src, json_object_get(spec_obj, #val))
/// --------------------------------

/// Formats
/// -------
unsigned int format_Bpp(format_t format)
{
	switch(format) {
		case FORMAT_BGRA8888:
			return 4;
		case FORMAT_ARGB4444:
		case FORMAT_RGB565:
			return 2;
		case FORMAT_GRAY8:
			return 1;
		default:
			log_printf("unknown format: %u\n", format);
			return 0;
	}
}

unsigned int format_png_equiv(format_t format)
{
	switch(format) {
		case FORMAT_BGRA8888:
		case FORMAT_ARGB4444:
		case FORMAT_RGB565:
			return PNG_FORMAT_BGRA;
		case FORMAT_GRAY8:
			return PNG_FORMAT_GRAY;
		default:
			log_printf("unknown format: %u\n", format);
			return 0;
	}
}

void format_from_bgra(png_bytep data, unsigned int pixels, format_t format)
{
	unsigned int i;
	png_bytep in = data;

	if(format == FORMAT_ARGB4444) {
		png_bytep out = data;
		for(i = 0; i < pixels; ++i, in += 4, out += 2) {
			// I don't see the point in doing any "rounding" here. Let's rather focus on
			// writing understandable code independent of endianness assumptions.
			const unsigned char b = in[0] >> 4;
			const unsigned char g = in[1] >> 4;
			const unsigned char r = in[2] >> 4;
			const unsigned char a = in[3] >> 4;
			// Yes, we start with the second byte. "Little-endian ARGB", mind you.
			out[1] = (a << 4) | r;
			out[0] = (g << 4) | b;
		}
	} else if(format == FORMAT_RGB565) {
		png_uint_16p out16 = (png_uint_16p)data;
		for(i = 0; i < pixels; ++i, in += 4, ++out16) {
			const unsigned char b = in[0] >> 3;
			const unsigned char g = in[1] >> 2;
			const unsigned char r = in[2] >> 3;

			out16[0] = (r << 11) | (g << 5) | b;
		}
	}
	// FORMAT_GRAY8 is fully handled by libpng
}
/// -------

int png_load_for_thtx(png_image_exp image, const char *fn, thtx_header_t *thtx)
{
	void *file_buffer = NULL;
	size_t file_size;

	if(!image || !thtx) {
		return -1;
	}

	SAFE_FREE(image->buf);
	png_image_free(&image->img);
	ZeroMemory(&image->img, sizeof(png_image));
	image->img.version = PNG_IMAGE_VERSION;

	if(strncmp(thtx->magic, "THTX", sizeof(thtx->magic))) {
		return 1;
	}

	file_buffer = stack_game_file_resolve(fn, &file_size);
	if(!file_buffer) {
		return 2;
	}

	if(png_image_begin_read_from_memory(&image->img, file_buffer, file_size)) {
		image->img.format = format_png_equiv(thtx->format);
		if(image->img.format) {
			size_t png_size = PNG_IMAGE_SIZE(image->img);
			image->buf = (png_bytep)malloc(png_size);

			if(image->buf) {
				png_image_finish_read(&image->img, 0, image->buf, 0, NULL);
			}
		}
	}
	SAFE_FREE(file_buffer);
	if(image->buf) {
		format_from_bgra(image->buf, image->img.width * image->img.height, thtx->format);
	}
	return 0;
}

// Patches an [image] prepared by <png_load_for_thtx> into [thtx], starting at [x],[y].
// [png] is assumed to have the same bit depth as [thtx].
int patch_thtx(thtx_header_t *thtx, const size_t x, const size_t y, png_image_exp image)
{
	if(!thtx || !image || !image->buf || x >= image->img.width || y >= image->img.height) {
		return -1;
	}
	{
		int bpp = format_Bpp(thtx->format);
		if(x == 0 && y == 0 && (thtx->w == image->img.width) && (thtx->h == image->img.height)) {
			// Optimization for the most frequent case
			memcpy(thtx->data, image->buf, thtx->size);
		} else {
			png_bytep in = image->buf + (y * image->img.width * bpp);
			png_bytep out = thtx->data;
			size_t png_stride = image->img.width * bpp;
			size_t thtx_stride = thtx->w * bpp;
			size_t row;
			for(row = 0; row < min(thtx->h, (image->img.height - y)); row++) {
				memcpy(out, in + (x * bpp), min(png_stride, thtx_stride));

				in += png_stride;
				out += thtx_stride;
			}
		}
	}
	return 0;
}

int patch_anm(BYTE *file_inout, size_t size_out, size_t size_in, json_t *patch, json_t *run_cfg)
{
	json_t *format = json_object_get(json_object_get(run_cfg, "formats"), "anm");
	json_t *dat_dump = json_object_get(run_cfg, "dat_dump");
	size_t headersize = json_object_get_hex(format, "headersize");

	// Some ANMs reference the same file name multiple times in a row
	const char *name_prev = NULL;

	png_image_ex png = {0};
	png_image_ex bounds = {0};

	BYTE *anm_entry_out = file_inout;

	if(!format) {
		return 1;
	}

	log_printf("---- ANM ----\n");

	if(!headersize) {
		log_printf("(no ANM header size given, sprite-local patching disabled)\n");
	}

	while(anm_entry_out < file_inout + size_in) {
		size_t x;
		size_t y;
		size_t nameoffset;
		size_t thtxoffset;
		size_t hasdata;
		size_t nextoffset;
		size_t sprites;

		if(
			STRUCT_GET(size_t, x, anm_entry_out, format) ||
			STRUCT_GET(size_t, y, anm_entry_out, format) ||
			STRUCT_GET(size_t, nameoffset, anm_entry_out, format) ||
			STRUCT_GET(size_t, thtxoffset, anm_entry_out, format) ||
			STRUCT_GET(size_t, hasdata, anm_entry_out, format) ||
			STRUCT_GET(size_t, nextoffset, anm_entry_out, format) ||
			STRUCT_GET(size_t, sprites, anm_entry_out, format)
		) {
			log_printf("Corrupt ANM file or format definition, aborting ...\n");
			break;
		}
		if(hasdata && thtxoffset) {
			const char *name = (const char*)(anm_entry_out + nameoffset);
			thtx_header_t *thtx = (thtx_header_t*)(anm_entry_out + thtxoffset);

			// Load a new replacement image, if necessary...
			if(!name_prev || strcmp(name, name_prev)) {
				png_load_for_thtx(&png, name, thtx);

				if(!json_is_false(dat_dump)) {
					bounds_store(name_prev, &bounds);
					bounds_init(&bounds, thtx, name);
				}
				name_prev = name;
			}
			// ... add texture boundaries...
			bounds_resize(&bounds, x + thtx->w, y + thtx->h);

			if(headersize) {
				size_t i;
				DWORD *sprite_ptr = (DWORD*)(anm_entry_out + headersize);
				for(i = 0; i < sprites; i++, sprite_ptr++) {
					sprite_t *sprite = (sprite_t*)(anm_entry_out + sprite_ptr[0]);
					bounds_draw_rect(&bounds, x, y, sprite);
				}
			}
			// ... and patch it.
			patch_thtx(thtx, x, y, &png);
		}
		if(!nextoffset) {
			bounds_store(name_prev, &bounds);
			break;
		}
		anm_entry_out += nextoffset;
	}
	SAFE_FREE(bounds.buf);
	SAFE_FREE(png.buf);
	log_printf("-------------\n");
	return 0;
}
