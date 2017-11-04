/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * PNG files reading.
  */

#include <thcrap.h>
#include <png.h>
#include "./png.h"

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
BYTE **png_image_read(const char *fn, uint32_t *width, uint32_t *height, uint8_t *bpp)
{
	stack_chain_iterate_t sci = { 0 };
	BYTE *file_buffer = nullptr;
	file_buffer_t file = { 0 };

	json_t *chain = resolve_chain_game(fn);

	if (json_array_size(chain)) {
		log_printf("(PNG) Resolving %s...", json_array_get_string(chain, 0));
		while (file_buffer == nullptr && stack_chain_iterate(&sci, chain, SCI_BACKWARDS) != 0) {
			file_buffer = (BYTE*)patch_file_load(sci.patch_info, sci.fn, &file.size);
		}
	}
	json_decref(chain);
	if (!file_buffer) {
		log_print("not found\n");
		return nullptr;
	}
	patch_print_fn(sci.patch_info, fn);
	log_print("\n");
	file.buffer = file_buffer;
	
	if (!png_check_sig(file.buffer, 8)) {
		log_print("Bad PNG signature!\n");
		free(file_buffer);
		return nullptr;
	}
	file.buffer += 8;
	file.size -= 8;

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, png_error_callback, png_warning_callback);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		free(file_buffer);
		return nullptr;
	}
	png_set_read_fn(png_ptr, &file, read_bytes);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);

	int bit_depth, color_type;
	png_get_IHDR(png_ptr, info_ptr, width, height, &bit_depth, &color_type, NULL, NULL, NULL);
	*bpp = (color_type & PNG_COLOR_MASK_ALPHA) ? 32 : 24;

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
	BYTE **row_pointers = (BYTE**)malloc(sizeof(BYTE*) * *height + rowbytes * *height);
	BYTE* image_data = ((BYTE*)row_pointers) + sizeof(BYTE*) * *height;
	for (uint32_t i = 0; i < *height; i++)
		row_pointers[i] = image_data + i * rowbytes;
	png_read_image(png_ptr, row_pointers);

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	free(file_buffer);
	return row_pointers;
}
