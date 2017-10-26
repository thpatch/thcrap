/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * New Super Marisa Land support functions
  */

#include <thcrap.h>
#include <png.h>
#include "thcrap_tasofro.h"

int patch_cv2(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t*);

int nsml_init()
{
	patchhook_register("*.cv2", patch_cv2);
	return 0;
}

struct file_buffer_t {
	BYTE *buffer;
	size_t size;
};

void read_bytes(png_structp png_ptr, png_bytep out, png_size_t out_size)
{
	file_buffer_t *in = (file_buffer_t*)png_get_io_ptr(png_ptr);
	if (out_size > in->size) {
		log_print("Error: file truncated!\n");
		longjmp(png_jmpbuf(png_ptr), 1);
	}
	memcpy(out, in->buffer, out_size);
	in->buffer += out_size;
	in->size   -= out_size;
}

void png_warning_callback(png_structp png_ptr, png_const_charp msg)
{
	log_printf("Warning: %s\n", msg);
}

void png_error_callback(png_structp png_ptr, png_const_charp msg)
{
	log_printf("Error: %s!\n", msg);
	longjmp(png_jmpbuf(png_ptr), 1);
}

// PNG reading core is adapted from http://www.libpng.org/pub/png/book/chapter13.html
int patch_cv2(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t*)
{
	BYTE *file_out = (BYTE*)file_inout;
	stack_chain_iterate_t sci = { 0 };
	BYTE *file_rep_buffer = nullptr;
	file_buffer_t file_rep = { 0 };

	VLA(char, fn_png, strlen(fn) + 1);
	strcpy(fn_png, fn);
	strcpy(PathFindExtensionA(fn_png), ".png");
	json_t *chain = resolve_chain_game(fn_png);

	if (json_array_size(chain)) {
		log_printf("(PNG) Resolving %s...", json_array_get_string(chain, 0));
		while (file_rep_buffer == nullptr && stack_chain_iterate(&sci, chain, SCI_BACKWARDS) != 0) {
			file_rep_buffer = (BYTE*)patch_file_load(sci.patch_info, sci.fn, &file_rep.size);
		}
	}
	if (!file_rep_buffer) {
		log_print("not found\n");
		json_decref(chain);
		return 0;
	}
	patch_print_fn(sci.patch_info, fn_png);
	log_print("\n");
	VLA_FREE(fn_png);
	file_rep.buffer = file_rep_buffer;
	
	if (!png_check_sig(file_rep.buffer, 8)) {
		log_print("Bad PNG signature!\n");
		free(file_rep_buffer);
		return -1;
	}
	file_rep.buffer += 8;
	file_rep.size -= 8;

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, png_error_callback, png_warning_callback);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		free(file_rep_buffer);
		return -1;
	}
	png_set_read_fn(png_ptr, &file_rep, read_bytes);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);

	uint32_t width, height;
	int bit_depth, color_type;
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);
	int bpp = (color_type & PNG_COLOR_MASK_ALPHA) ? 4 : 3; // Bytes per pixels, not bits per pixels

	if (17 + width * height * 4 > size_out) {
		log_print("Destination buffer too small!\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		free(file_rep_buffer);
		return -1;
	}

	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_expand(png_ptr);
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand(png_ptr);
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_expand(png_ptr);
	if (bit_depth == 16)
		png_set_strip_16(png_ptr);
	if (color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	uint32_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
	VLA(BYTE*, row_pointers, height);
	BYTE* image_data = (BYTE*)malloc(rowbytes * height);
	for (uint32_t i = 0; i < height; i++)
		row_pointers[i] = image_data + i * rowbytes;
	png_read_image(png_ptr, row_pointers);

	file_out[0] = bpp * 8;
	DWORD *header = (DWORD*)(file_out + 1);
	header[0] = width;
	header[1] = height;
	header[2] = width;
	header[3] = 0;
	for (unsigned int h = 0; h < height; h++) {
		for (unsigned int w = 0; w < width; w++) {
			file_out[17 + h * width * 4 + w * 4 + 0] = row_pointers[h][w * bpp + 2];
			file_out[17 + h * width * 4 + w * 4 + 1] = row_pointers[h][w * bpp + 1];
			file_out[17 + h * width * 4 + w * 4 + 2] = row_pointers[h][w * bpp + 0];
			if (bpp == 4) {
				file_out[17 + h * width * 4 + w * 4 + 3] = row_pointers[h][w * bpp + 3];
			}
			else {
				file_out[17 + h * width * 4 + w * 4 + 3] = 0xFF;
			}
		}
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	VLA_FREE(row_pointers);
	free(file_rep_buffer);
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
	json_object_set_new(new_bp_info, "post_read", json_integer((json_int_t)nsml_patch));
	json_object_set_new(new_bp_info, "post_patch", json_integer((json_int_t)nsml_patch));
	int ret = BP_fragmented_read_file(regs, new_bp_info);
	json_decref(new_bp_info);
	free(uFilename);
	return ret;
}
