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
typedef enum {
    FORMAT_BGRA8888 = 1,
    FORMAT_BGR565 = 3,
    FORMAT_BGRA4444 = 5,
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

int patch_thtx(thtx_header_t *thtx, size_t x, size_t y, void *rep_buffer, size_t rep_size)
{
	png_image rep_png;
	png_bytep rep_png_buffer = NULL;

	if(!thtx || !rep_buffer) {
		return -1;
	}
	if(strncmp(thtx->magic, "THTX", sizeof(thtx->magic))) {
		return -2;
	}
	// Only support for BGRA8888, single-texture images right now
	if(thtx->format != FORMAT_BGRA8888) {
		log_printf("No patching support for texture format %d at this point.\n", thtx->format);
		return 1;
	} else if(x || y) {
		log_printf("No patching support for multi-texture images at this point.\n");
		return 1;
	}

	ZeroMemory(&rep_png, sizeof(png_image));
	rep_png.version = PNG_IMAGE_VERSION;

	if(png_image_begin_read_from_memory(&rep_png, rep_buffer, rep_size)) {
		size_t rep_png_size;
		
		rep_png.format = PNG_FORMAT_BGRA;
		rep_png_size = PNG_IMAGE_SIZE(rep_png);
		if(rep_png_size == thtx->size) {
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

	// Do format conversions on the rep_png_buffer...

	memcpy(thtx->data, rep_png_buffer, thtx->size);

	SAFE_FREE(rep_png_buffer);
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
	return 1;
}
