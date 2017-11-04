/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * New Super Marisa Land support functions
  */

#include <thcrap.h>
#include "./png.h"
#include "thcrap_tasofro.h"

int patch_cv2(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t*);

int nsml_init()
{
	patchhook_register("*.cv2", patch_cv2);
	return 0;
}

int patch_cv2(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t*)
{
	BYTE *file_out = (BYTE*)file_inout;
	VLA(char, fn_png, strlen(fn) + 1);
	strcpy(fn_png, fn);
	strcpy(PathFindExtensionA(fn_png), ".png");

	uint32_t width, height;
	uint8_t bpp;
	BYTE** row_pointers = png_image_read(fn_png, &width, &height, &bpp);
	VLA_FREE(fn_png);

	if (!row_pointers) {
		return 0;
	}

	if (17 + width * height * 4 > size_out) {
		log_print("Destination buffer too small!\n");
		free(row_pointers);
		return -1;
	}

	file_out[0] = bpp;
	DWORD *header = (DWORD*)(file_out + 1);
	header[0] = width;
	header[1] = height;
	header[2] = width;
	header[3] = 0;
	for (unsigned int h = 0; h < height; h++) {
		for (unsigned int w = 0; w < width; w++) {
			file_out[17 + h * width * 4 + w * 4 + 0] = row_pointers[h][w * bpp / 8 + 2];
			file_out[17 + h * width * 4 + w * 4 + 1] = row_pointers[h][w * bpp / 8 + 1];
			file_out[17 + h * width * 4 + w * 4 + 2] = row_pointers[h][w * bpp / 8 + 0];
			if (bpp == 32) {
				file_out[17 + h * width * 4 + w * 4 + 3] = row_pointers[h][w * bpp / 8 + 3];
			}
			else {
				file_out[17 + h * width * 4 + w * 4 + 3] = 0xFF;
			}
		}
	}

	free(row_pointers);
	return 1;
}

int BP_nsml_file_header(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	const char *filename = (const char*)json_object_get_immediate(bp_info, regs, "file_name");
	// ----------

	char *uFilename = EnsureUTF8(filename, strlen(filename));
	CharLowerA(uFilename);

	json_t *new_bp_info = json_copy(bp_info);
	json_object_set_new(new_bp_info, "file_name", json_integer((json_int_t)uFilename));
	int ret = BP_file_header(regs, new_bp_info);
	json_decref(new_bp_info);
	free(uFilename);
	return ret;
}

static void megamari_patch(const file_rep_t *fr, BYTE *buffer, size_t size)
{
	BYTE key = ((fr->offset >> 1) | 8) & 0xFF;
	for (unsigned int i = 0; i < size; i++) {
		buffer[i] ^= key;
	}
}

static void nsml_patch(const file_rep_t *fr, BYTE *buffer, size_t size)
{
	BYTE key = ((fr->offset >> 1) | 0x23) & 0xFF;
	for (unsigned int i = 0; i < size; i++) {
		buffer[i] ^= key;
	}
}

int BP_nsml_read_file(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	const char *filename = (const char*)json_object_get_immediate(bp_info, regs, "file_name");
	// ----------

	char *uFilename = EnsureUTF8(filename, strlen(filename));
	CharLowerA(uFilename);

	json_t *new_bp_info = json_copy(bp_info);
	json_object_set_new(new_bp_info, "file_name", json_integer((json_int_t)uFilename));
	if (game_id == TH_MEGAMARI) {
		json_object_set_new(new_bp_info, "post_read", json_integer((json_int_t)megamari_patch));
		json_object_set_new(new_bp_info, "post_patch", json_integer((json_int_t)megamari_patch));
	}
	else {
		json_object_set_new(new_bp_info, "post_read", json_integer((json_int_t)nsml_patch));
		json_object_set_new(new_bp_info, "post_patch", json_integer((json_int_t)nsml_patch));
	}
	int ret = BP_fragmented_read_file(regs, new_bp_info);
	json_decref(new_bp_info);
	free(uFilename);
	return ret;
}
