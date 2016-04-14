/**
* Touhou Community Reliant Automatic Patcher
* Team Shanghai Alice support plugin
*
* ----
*
* Th06-specific breakpoints around the standard file breakpoints.
*
* In th06, some PNG files needs to be splitted into 2 files: a RGB image
* (using a palette of at most 256 entries), and an alpha mask (a RGB image
* using black for fully opaque, white for fully transparent, and grayscale
* for semi-transparent, with a palette of at most 16 entries).
* For these images, a different set of functions will be called.
*/

#include <thcrap.h>
#include <png.h>
#include <bp_file.h>
#include "thcrap_tsa.h"
#include "th06_pngsplit.h"

#define PNGSPLIT_SAFE_FREE(x) \
	if(x) { \
		pngsplit_free(x); \
		x = NULL; \
	}

typedef enum {
	TH06_PNGSPLIT_NONE = -1,
	TH06_PNGSPLIT_RGB = 0,
	TH06_PNGSPLIT_ALPHA = 1,
} th06_pngsplit_t;

// Maximum size of the generated image.
const size_t pngsplit_max_image_size =
	8 // Magic
	+ 12 + 13 // IDHR
	+ 12 + 3 * 256 // PLTE
	+ 12 + 257 * 256 * 3 // IDAT
	+ 12; // IEND

th06_pngsplit_t pngsplit_state;
void *pngsplit_rep_buffer;
void *pngsplit_png = NULL;

int BP_th06_file_name(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	char **file_name = (char**)json_object_get_register(bp_info, regs, "file_name");
	// ----------

	if (file_name && !stricmp(PathFindExtensionA(*file_name), ".png")) {
		if (pngsplit_state == TH06_PNGSPLIT_NONE) {
			pngsplit_state = TH06_PNGSPLIT_RGB;
		}
	}
	else {
		PNGSPLIT_SAFE_FREE(pngsplit_png);
		pngsplit_state = TH06_PNGSPLIT_NONE;
	}
	return BP_file_name(regs, bp_info);
}

int BP_th06_file_size(x86_reg_t *regs, json_t *bp_info)
{
	if (pngsplit_state != TH06_PNGSPLIT_NONE) {
		// Ensure we'll have enough space for the patched PNG file
		size_t *file_size = (size_t*)json_object_get_register(bp_info, regs, "file_size");
		file_rep_t *fr = fr_tls_get();

		if (*file_size < pngsplit_max_image_size) {
			*file_size = pngsplit_max_image_size;
		}
		if (*file_size < fr->pre_json_size) {
			*file_size = fr->pre_json_size;
		}
		return 1;
	}
	return BP_file_size(regs, bp_info);
}

int BP_th06_file_load(x86_reg_t *regs, json_t *bp_info)
{
	if (pngsplit_state == TH06_PNGSPLIT_ALPHA) {
		file_rep_t *fr = fr_tls_get();

		if (fr->rep_buffer != NULL) { // If the original image is an alpha mask, the file will be replaced without conversion.
			PNGSPLIT_SAFE_FREE(pngsplit_png);
			pngsplit_state = TH06_PNGSPLIT_RGB;
		} else {
			pngsplit_rep_buffer = fr->rep_buffer;
			fr->rep_buffer = NULL; // Make the game load the file normally
		}
	}
	if (pngsplit_state == TH06_PNGSPLIT_RGB) {
		file_rep_t *fr = fr_tls_get();

		if (fr->rep_buffer == NULL) { // Nothing to do.
			PNGSPLIT_SAFE_FREE(pngsplit_png);
			pngsplit_state = TH06_PNGSPLIT_NONE;
			return BP_file_load(regs, bp_info);
		}

		pngsplit_rep_buffer = fr->rep_buffer;
		fr->rep_buffer = NULL; // Make the game load the file normally
	}
	return BP_file_load(regs, bp_info);
}

int th06_skip_image(file_rep_t *fr)
{
	memcpy(fr->game_buffer, fr->rep_buffer, fr->pre_json_size);
	file_rep_clear(fr);
	PNGSPLIT_SAFE_FREE(pngsplit_png);
	pngsplit_state = TH06_PNGSPLIT_NONE;
	return 1;
}

int BP_th06_file_loaded(x86_reg_t *regs, json_t *bp_info)
{
	if (pngsplit_state == TH06_PNGSPLIT_RGB || pngsplit_state == TH06_PNGSPLIT_ALPHA) {
		file_rep_t *fr = fr_tls_get();
		fr->rep_buffer = pngsplit_rep_buffer;
		pngsplit_rep_buffer = NULL;

		// Other breakpoints
		// -----------------
		BP_file_buffer(regs, bp_info);
		// -----------------

		// If we don't have a game buffer, we can't do anything.
		if (!fr->game_buffer) {
			PNGSPLIT_SAFE_FREE(pngsplit_png);
			pngsplit_state = TH06_PNGSPLIT_NONE;
			return BP_file_loaded(regs, bp_info);
		}

		png_byte orig_bit_depth = ((png_bytep)fr->game_buffer)[8 /* magic */ + 8 /* chunk header */ + 8 /* index of bit depth in IHRD */];
		png_byte orig_color_type = ((png_bytep)fr->game_buffer)[8 /* magic */ + 8 /* chunk header */ + 9 /* index of color type in IHRD */];
		if (orig_color_type != PNG_COLOR_TYPE_RGB && orig_color_type != PNG_COLOR_TYPE_PALETTE) { // I don't think the game uses another color type. Maybe RGBA, and in that case the input file will probably be RGBA as well.
			return th06_skip_image(fr);
		}

		if (pngsplit_state == TH06_PNGSPLIT_ALPHA) {
			if (fr->name && strlen(fr->name) >= 6 &&
				!strcmp(fr->name + strlen(fr->name) - 6, "_a.png")) {
				// Compute the alpha mask
				log_print("(PNG) Computing alpha mask\n");
				void *dst;
				dst = pngsplit_create_png_mask(pngsplit_png);
				if (!dst) {
					log_print("(PNG) Error\n");
					PNGSPLIT_SAFE_FREE(pngsplit_png);
					pngsplit_state = TH06_PNGSPLIT_NONE;
					return BP_file_loaded(regs, bp_info);
				}
				pngsplit_write(fr->game_buffer, dst);
				dst = NULL;

				PNGSPLIT_SAFE_FREE(pngsplit_png);
				pngsplit_state = TH06_PNGSPLIT_NONE;
				file_rep_clear(fr);
				return 1;
			} else { // The original image may be a RGB image. Let's try that.
				PNGSPLIT_SAFE_FREE(pngsplit_png);
				pngsplit_state = TH06_PNGSPLIT_RGB;
			}
		}

		if (pngsplit_state == TH06_PNGSPLIT_RGB)
		{
			// If we have an alpha mask here, that means the patch developer tries to replace the image for the alpha mask, and... let's hope he knows what he does.
			if (orig_color_type == PNG_COLOR_TYPE_PALETTE && orig_bit_depth != 8) {
				return th06_skip_image(fr);
			}

			// Do the splitting
			pngsplit_png = pngsplit_read(fr->rep_buffer);
			if (!pngsplit_png) {
				log_print("(PNG) Error reading file\n");
				pngsplit_state = TH06_PNGSPLIT_NONE;
				return BP_file_loaded(regs, bp_info);
			}


			log_print("(PNG) Computing indexed image\n");
			void* dst;
			dst = pngsplit_create_rgb_file(pngsplit_png);
			if (!dst) {
				log_print("(PNG) Error\n");
				PNGSPLIT_SAFE_FREE(pngsplit_png);
				pngsplit_state = TH06_PNGSPLIT_NONE;
				return BP_file_loaded(regs, bp_info);
			}
			pngsplit_write(fr->game_buffer, dst);
			dst = NULL;

			file_rep_clear(fr);
			pngsplit_state = TH06_PNGSPLIT_ALPHA;
			return 1;
		}
	}
	return BP_file_loaded(regs, bp_info);
}
