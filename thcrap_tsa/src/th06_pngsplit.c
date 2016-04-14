/**
* Touhou Community Reliant Automatic Patcher
* Team Shanghai Alice support plugin
*
* ----
*
* Png splitter for th06 png files.
*/

#include <thcrap.h>
#include <stdlib.h>
#include <stdio.h>
#include <png.h>

#define ABS(x) ((x) >= 0 ? (x) : (x) * -1)

typedef enum {
	PNGSPLIT_ALLOC_READ,
	PNGSPLIT_ALLOC_WRITE
} pngsplit_alloc_type_t;

typedef struct {
	pngsplit_alloc_type_t alloc_type;
	png_structp png;
	png_infop info;
	png_uint_32 width, height;
	png_bytep data;
	png_bytepp row_pointers;
} pngsplit_png_t;

typedef struct {
	void *buff;
	int pos;
} pngsplit_io_t;

pngsplit_png_t *pngsplit_alloc(pngsplit_alloc_type_t alloc_type)
{
	pngsplit_png_t *png = malloc(sizeof(pngsplit_png_t));
	if (!png)
		return NULL;
	ZeroMemory(png, sizeof(pngsplit_png_t));
	png->alloc_type = alloc_type;

	if (alloc_type == PNGSPLIT_ALLOC_READ) {
		png->png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	} else if (alloc_type == PNGSPLIT_ALLOC_WRITE) {
		png->png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	}
	if (!png->png) {
		free(png);
		return NULL;
	}

	png->info = png_create_info_struct(png->png);
	if (!png->info) {
		png_destroy_read_struct(&png->png, NULL, NULL);
		free(png);
		return NULL;
	}

	return png;
}

int pngsplit_alloc_data(pngsplit_png_t *png)
{
	if (!png || png->width == 0 || png->height == 0) {
		return -1;
	}
	int rowbytes = png_get_rowbytes(png->png, png->info);
	png->data = malloc(rowbytes * png->height);
	png->row_pointers = malloc(sizeof(png_bytep) * png->height);
	if (!png->data || !png->row_pointers) {
		SAFE_FREE(png->data);
		SAFE_FREE(png->row_pointers);
		return -1;
	}
	for (png_uint_32 i = 0; i < png->height; i++) {
		png->row_pointers[i] = png->data + i * rowbytes;
	}
	return 0;
}

void pngsplit_free(pngsplit_png_t *png)
{
	if (png) {
		SAFE_FREE(png->data);
		SAFE_FREE(png->row_pointers);

		if (png->png) {
			if (png->alloc_type == PNGSPLIT_ALLOC_READ) {
				if (png->info) {
					png_destroy_read_struct(&png->png, &png->info, NULL);
				} else {
					png_destroy_read_struct(&png->png, NULL, NULL);
				}
			} else {
				if (png->info) {
					png_destroy_write_struct(&png->png, &png->info);
				} else {
					png_destroy_write_struct(&png->png, NULL);
				}
			}
		}
		free(png);
	}
}




void pngsplit_read_callback(png_struct *png, png_bytep out, png_uint_32 len)
{
	pngsplit_io_t *io = png_get_io_ptr(png);
	memcpy(out, (char*)io->buff + io->pos, len);
	io->pos += len;
}

pngsplit_png_t *pngsplit_read(void *buff)
{
	int err = -1;

	// Initialize stuff
	pngsplit_png_t *png = pngsplit_alloc(PNGSPLIT_ALLOC_READ);
	if (!png)
		return NULL;
	if (!setjmp(png_jmpbuf(png->png))) {
		pngsplit_io_t io;
		io.buff = buff;
		io.pos = 0;
		png_set_read_fn(png->png, &io, pngsplit_read_callback);

		// Read header
		png_read_info(png->png, png->info);

		int bit_depth, color_type;
		png_get_IHDR(png->png, png->info, &png->width, &png->height, &bit_depth, &color_type, NULL, NULL, NULL);

		// Tranform to 32-bits RGBA
		if (bit_depth == 16) {
			png_set_strip_16(png->png);
		}
		png_set_expand(png->png);
		if (color_type == PNG_COLOR_TYPE_RGB ||
			color_type == PNG_COLOR_TYPE_GRAY ||
			color_type == PNG_COLOR_TYPE_PALETTE) {
			png_set_add_alpha(png->png, 0xFF, PNG_FILLER_AFTER);
		}
		if (color_type == PNG_COLOR_TYPE_GRAY ||
			color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
			png_set_gray_to_rgb(png->png);
		}
		png_read_update_info(png->png, png->info);

		// Read the image
		if (pngsplit_alloc_data(png) != -1) {
			png_read_image(png->png, png->row_pointers);
			err = 0;
		}
	}

	if (err == -1) {
		pngsplit_free(png);
		png = NULL;
	}

	return png;
}

void pngsplit_write_callback(png_struct *png, png_bytep in, png_uint_32 len)
{
	pngsplit_io_t *io = png_get_io_ptr(png);
	memcpy((char*)io->buff + io->pos, in, len);
	io->pos += len;
}

void pngsplit_write_flush_callback(png_struct *png)
{
	(void)png;
}

void pngsplit_write(char *buff, pngsplit_png_t *png)
{
	if (!setjmp(png_jmpbuf(png->png))) {
		pngsplit_io_t io;
		io.buff = buff;
		io.pos = 0;
		png_set_write_fn(png->png, &io, pngsplit_write_callback, pngsplit_write_flush_callback);

		png_write_info(png->png, png->info);

		png_write_image(png->png, png->row_pointers);
		png_write_end(png->png, NULL);
	}

	pngsplit_free(png);
}






pngsplit_png_t *pngsplit_create_png_mask(pngsplit_png_t *in)
{
	int err = -1;

	pngsplit_png_t *out = pngsplit_alloc(PNGSPLIT_ALLOC_WRITE);
	if (!out)
		return NULL;
	if (!setjmp(png_jmpbuf(out->png))) {
		out->width = in->width; out->height = in->height;
		png_set_IHDR(out->png,
			out->info,
			out->width, out->height,
			8,
			PNG_COLOR_TYPE_RGB,
			PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT
			);

		if (pngsplit_alloc_data(out) != -1) {
			for (png_uint_32 y = 0; y < in->height; y++) {
				png_bytep row = in->row_pointers[y];
				for (png_uint_32 x = 0; x < in->width; x++) {
					png_bytep px = &(row[x * 4]);
					out->row_pointers[y][x * 3 + 0] = px[3];
					out->row_pointers[y][x * 3 + 1] = px[3];
					out->row_pointers[y][x * 3 + 2] = px[3];
				}
			}
			err = 0;
		}
	}

	if (err == -1) {
		pngsplit_free(out);
		out = NULL;
	}

	return out;
}






pngsplit_png_t *pngsplit_create_rgb_file(pngsplit_png_t *in)
{
	int err = -1;

	pngsplit_png_t *out;
	out = pngsplit_alloc(PNGSPLIT_ALLOC_WRITE);
	if (out == NULL) {
		return NULL;
	}

	if (!setjmp(png_jmpbuf(out->png))) {
		out->width = in->width; out->height = in->height;
		png_set_IHDR(out->png,
			out->info,
			out->width, out->height,
			8,
			PNG_COLOR_TYPE_RGB,
			PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT
			);

		if (pngsplit_alloc_data(out) != -1) {
			for (png_uint_32 y = 0; y < in->height; y++) {
				png_bytep row = in->row_pointers[y];
				for (png_uint_32 x = 0; x < in->width; x++) {
					png_bytep px = &(row[x * 4]); // px is in RGBA BE format.
					out->row_pointers[y][x * 3 + 0] = px[0];
					out->row_pointers[y][x * 3 + 1] = px[1];
					out->row_pointers[y][x * 3 + 2] = px[2];
				}
			}
			err = 0;
		}
	}

	if (err == -1) {
		pngsplit_free(out);
		out = NULL;
	}

	return out;
}
