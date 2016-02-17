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
	png_colorp plt;
	int plt_size;
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
		SAFE_FREE(png->plt);

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

		png_set_PLTE(png->png, png->info, png->plt, png->plt_size);
		png_write_info(png->png, png->info);

		png_set_compression_level(png->png, 9);
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
			4,
			PNG_COLOR_TYPE_PALETTE,
			PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT
			);
		out->plt = malloc(16 * sizeof(png_color));
		out->plt_size = 16;
		for (int i = 0; i < 16; i++) {
			png_byte color = i + (i << 4);
			out->plt[i].red = color;
			out->plt[i].green = color;
			out->plt[i].blue = color;
		}

		if (out->plt && pngsplit_alloc_data(out) != -1) {
			for (png_uint_32 y = 0; y < in->height; y++) {
				png_bytep row = in->row_pointers[y];
				for (png_uint_32 x = 0; x < in->width; x++) {
					png_bytep px = &(row[x * 4]);
					if (x % 2 == 0) {
						out->row_pointers[y][x / 2] &= 0x0F;
						out->row_pointers[y][x / 2] |= px[3] & 0xF0;
					} else {
						out->row_pointers[y][x / 2] &= 0xF0;
						out->row_pointers[y][x / 2] |= px[3] >> 4;
					}
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






static png_uint_32 *build_palette(pngsplit_png_t *png, int *plt_size)
{
	png_uint_32* plt = NULL;
	int alloc_size = 0;
	*plt_size = 0;

	for (png_uint_32 y = 0; y < png->height; y++) {
		png_uint_32* row = (png_uint_32*)png->row_pointers[y];
		for (png_uint_32 x = 0; x < png->width; x++) {
			png_uint_32 px = row[x]; // px is in RGBA BE format.
			px &= 0x00FFFFFF; // Removing alpha

			int i;
			for (i = 0; i < *plt_size; i++) {
				if (plt[i] == px) {
					break;
				}
			}
			if (i == *plt_size) {
				if (i == alloc_size) {
					if (alloc_size == 0) {
						alloc_size = 256;
					} else {
						alloc_size <<= 1;
					}
					plt = realloc(plt, alloc_size * sizeof(png_uint_32));
					if (plt == NULL) {
						return NULL;
					}
				}
				plt[i] = px;
				(*plt_size)++;
			}
		}
	}
	return plt;
}

static png_uint_32 *build_assoc_table(png_uint_32 *plt, int plt_size, int *assoc_table_size)
{
	if (plt == NULL) {
		return NULL;
	}

	png_uint_32 *assoc_table = malloc(plt_size * 2 * sizeof(png_uint_32));
	if (assoc_table == NULL) {
		return NULL;
	}
	*assoc_table_size = 0;
	int nb_colors = plt_size, shift = 1;

	while (nb_colors > 256) {
		for (int i = 0; i < plt_size; i++) {
			png_bytep px1 = (png_bytep)(plt + i);
			if (px1[3] == 0xFF) {
				continue;
			}
			for (int j = i + 1; j < plt_size; j++) {
				png_bytep px2 = (png_bytep)(plt + j);
				if (px2[3] != 0xFF && ABS(px1[0] - px2[0]) + ABS(px1[1] - px2[1]) + ABS(px1[2] - px2[2]) <= shift) {
					assoc_table[*assoc_table_size] = plt[j];
					assoc_table[*assoc_table_size + 1] = plt[i];
					(*assoc_table_size) += 2;
					px2[3] = 0xFF;
					nb_colors--;
				}
			}
		}
		shift++;
	}
	return assoc_table;
}

static void sort_assoc_table(png_uint_32 *assoc, int assoc_size)
{
	for (int i = 2; i < assoc_size; ) {
		if (assoc[i - 2] > assoc[i]) {
			png_uint_32 tmp;
			tmp = assoc[i - 2];
			assoc[i - 2] = assoc[i];
			assoc[i] = tmp;
			tmp = assoc[i - 1];
			assoc[i - 1] = assoc[i + 1];
			assoc[i + 1] = tmp;
			if (i != 2)
				i -= 2;
		}
		else
			i += 2;
	}
}

static UINT32 get_assoc(UINT32 px, UINT32* assoc_table, int assoc_table_size)
{
	int min, max, mid;
	UINT32 cur;

	min = 0;
	max = assoc_table_size / 2 - 1;
	while (1)
	{
		if (max - min < 10)
		{
			for (int i = min; i <= max; i++)
				if (assoc_table[i * 2] == px)
				{
					px = assoc_table[i * 2 + 1];
					return get_assoc(assoc_table[i * 2 + 1], assoc_table, assoc_table_size);
				}
			return px;
		}
		mid = (max - min) / 2 + min;
		cur = assoc_table[mid * 2];
		if (cur == px)
			return get_assoc(assoc_table[mid * 2 + 1], assoc_table, assoc_table_size);
		else if (cur < px)
			min = mid;
		else if (cur > px)
			max = mid;
	}
}

pngsplit_png_t *pngsplit_create_rgb_file(pngsplit_png_t *in)
{
	png_uint_32 *plt = NULL, *assoc_table = NULL;
	int plt_size, assoc_table_size;
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
			PNG_COLOR_TYPE_PALETTE,
			PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT
			);

		plt = build_palette(in, &plt_size);
		assoc_table = build_assoc_table(plt, plt_size, &assoc_table_size);
		sort_assoc_table(assoc_table, assoc_table_size);

		out->plt = malloc(plt_size * sizeof(png_color));
		out->plt_size = 0;
		if (plt && assoc_table && out->plt) {
			for (int i = 0; i < plt_size; i++) {
				if ((plt[i] & 0xFF000000) == 0) {
					png_bytep px = (png_bytep)(plt + i);
					out->plt[out->plt_size].red = px[0];
					out->plt[out->plt_size].green = px[1];
					out->plt[out->plt_size].blue = px[2];
					out->plt_size++;
				}
			}

			pngsplit_alloc_data(out);
			for (png_uint_32 y = 0; y < in->height; y++) {
				png_uint_32 *row = (png_uint_32*)in->row_pointers[y];
				for (png_uint_32 x = 0; x < in->width; x++) {
					png_uint_32 px = row[x]; // px is in RGBA BE format.
					px &= 0x00FFFFFF; // Remove alpha
					px = get_assoc(px, assoc_table, assoc_table_size);

					png_bytep px_ptr = (png_bytep)&px;
					int idx;
					for (idx = 0; idx < out->plt_size; idx++) {
						if (px_ptr[0] == out->plt[idx].red && px_ptr[1] == out->plt[idx].green && px_ptr[2] == out->plt[idx].blue) {
							break;
						}
					}
					if (idx == out->plt_size) {
						printf("Error in pngsplit: color %X not found in palette. Defaulting to index 0.\n", px);
						idx = 0;
					}
					out->row_pointers[y][x] = idx;
				}
			}
			err = 0;
		}
	}

	SAFE_FREE(plt);
	SAFE_FREE(assoc_table);
	if (err == -1) {
		pngsplit_free(out);
		out = NULL;
	}

	return out;
}
