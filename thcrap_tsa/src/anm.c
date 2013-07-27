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
#include "thcrap_tsa.h"
#include <png.h>

/// Enums
/// -----
// All of the 16-bit formats are little-endian.
typedef enum {
	FORMAT_BGRA8888 = 1,
	FORMAT_RGB565 = 3,
	// 0xGB 0xAR
	FORMAT_ARGB4444 = 5,
	FORMAT_GRAY8 = 7
} format_t;
/// -----

/// Structures
/// ----------
typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    char magic[4];
    WORD zero;
    WORD format;
    /* These may be different from the parent entry. */
    WORD w, h;
    DWORD size;
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
    unsigned char data[];
} PACK_ATTRIBUTE thtx_header_t;
/// ----------

/// JSON-based structure data access
/// --------------------------------
int struct_get(void *dest, size_t dest_size, void *src, json_t *spec)
{
	if(!dest || !dest_size || !src || !spec || !spec) {
		return -1;
	}
	{
		json_t *spec_offset = json_object_get(spec, "offset");
		json_t *spec_size = json_object_get(spec, "size");
		size_t offset = json_hex_value(spec_offset);
		// Default to architecture word size
		size_t size = sizeof(size_t);
		if(spec_size) {
			size = json_hex_value(spec_size);
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
	struct_get(&(val), sizeof(type), anm_entry_out, json_object_get(spec_obj, #val))
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

// Converts a number of BGRA8888 [pixels] in [data] to the given [format] in-place.
void format_from_bgra(png_bytep data, unsigned int pixels, format_t format)
{
	unsigned int i = 0;
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

int patch_thtx(thtx_header_t *thtx, size_t x, size_t y, void *rep_buffer, size_t rep_size)
{
	png_image rep_png;
	png_bytep rep_png_buffer = NULL;
	int format_bpp;

	if(!thtx || !rep_buffer) {
		return -1;
	}
	if(strncmp(thtx->magic, "THTX", sizeof(thtx->magic))) {
		return -2;
	}

	format_bpp = format_Bpp(thtx->format);
	if(!format_bpp) {
		return 1;
	} else if(x || y) {
		// Only support for single-texture images right now
		log_printf("No patching support for multi-texture images at this point.\n");
		return 1;
	}

	ZeroMemory(&rep_png, sizeof(png_image));
	rep_png.version = PNG_IMAGE_VERSION;

	if(png_image_begin_read_from_memory(&rep_png, rep_buffer, rep_size)) {
		size_t rep_png_size;
		
		rep_png.format = format_png_equiv(thtx->format);
		rep_png_size = PNG_IMAGE_SIZE(rep_png);
		if(rep_png.width * rep_png.height * format_bpp == thtx->size) {
			rep_png_buffer = (png_bytep)malloc(rep_png_size);

			if(rep_png_buffer) {
				png_image_finish_read(&rep_png, 0, rep_png_buffer, 0, NULL);
			}
		} else {
			log_printf(
				"Image size mismatch, patching not supported yet!\n"
				"\t(format: %d, original: %d, replacement: %d)\n",
				thtx->format, thtx->size, rep_png_size
			);
		}
	}

	if(rep_png_buffer) {
		format_from_bgra(rep_png_buffer, rep_png.width * rep_png.height, thtx->format);

		memcpy(thtx->data, rep_png_buffer, thtx->size);

		SAFE_FREE(rep_png_buffer);
	}
	png_image_free(&rep_png);
	return 0;
}

int patch_anm(BYTE *file_inout, size_t size_out, size_t size_in, json_t *patch, json_t *run_cfg)
{
	json_t *format;

	// Some ANMs reference the same file name multiple times in a row
	char *name_prev = NULL;
	void *rep_buffer = NULL;
	size_t rep_size;

	BYTE *anm_entry_out = file_inout;

	format = json_object_get(json_object_get(run_cfg, "formats"), "anm");

	log_printf("---- ANM ----\n");

	while(anm_entry_out < file_inout + size_in) {
		size_t x;
		size_t y;
		size_t nameoffset;
		size_t thtxoffset;
		size_t hasdata;
		size_t nextoffset;

		if(
			STRUCT_GET(size_t, x, anm_entry_out, format) ||
			STRUCT_GET(size_t, y, anm_entry_out, format) ||
			STRUCT_GET(size_t, nameoffset, anm_entry_out, format) ||
			STRUCT_GET(size_t, thtxoffset, anm_entry_out, format) ||
			STRUCT_GET(size_t, hasdata, anm_entry_out, format) ||
			STRUCT_GET(size_t, nextoffset, anm_entry_out, format)
		) {
			log_printf("Corrupt ANM file or format definition, aborting ...\n");
			return -2;
		}

		if(hasdata && thtxoffset) {
			char *name = (char*)(anm_entry_out + nameoffset);
			thtx_header_t *thtx = (thtx_header_t*)(anm_entry_out + thtxoffset);

			if(!name_prev || strcmp(name, name_prev)) {
				SAFE_FREE(rep_buffer);
				rep_buffer = stack_game_file_resolve(name, &rep_size);
				name_prev = name;
			}
			if(rep_buffer) {
				patch_thtx(thtx, x, y, rep_buffer, rep_size);
			}
		}

		if(!nextoffset) {
			break;
		} else {
			anm_entry_out += nextoffset;
		}
	}
	SAFE_FREE(rep_buffer);
	log_printf("-------------\n");
	return 1;
}
