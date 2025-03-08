/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Image patching for nsml engine.
  */

#include <thcrap.h>
#include "./png.h"
#include "thcrap_tasofro.h"
#include "nsml_images.h"

size_t get_image_data_size(const char *fn, bool fill_alpha_for_24bpp)
{
	STRDUP_VLA(char, fn_png, fn);
	strcpy(PathFindExtensionA(fn_png), ".png");

	uint32_t width, height, rowbytes;
	uint8_t bpp;
	bool IHDR_read = png_image_get_IHDR(fn_png, &width, &height, &bpp);
	VLA_FREE(fn_png);

	if (!IHDR_read) {
		return 0;
	}

	if (fill_alpha_for_24bpp) {
		rowbytes = width * 4;
	}
	else {
		rowbytes = width * bpp / 8;
		if (rowbytes % 4 != 0) {
			rowbytes += 4 - (rowbytes % 4);
		}
	}
	return rowbytes * height;
}

size_t get_cv2_size(const char *fn, json_t*, size_t)
{
	return 17 + get_image_data_size(fn, true);
}

int patch_cv2(void *file_inout, size_t size_out, size_t, const char *fn, json_t*)
{
	BYTE *file_out = (BYTE*)file_inout;
	STRDUP_VLA(char, fn_png, fn);
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
	for (size_t h = 0; h < height; h++) {
		for (size_t w = 0; w < width; w++) {
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

struct DatDesc {
	DWORD x;
	DWORD y;
	DWORD w;
	DWORD h;
};
// Ensure the compiler doesn't add any padding
static_assert(sizeof(DatDesc) == 4 * sizeof(DWORD));

int patch_dat_for_png(void *file_inout, size_t, size_t size_in, const char *fn, json_t*)
{
	if (size_in < 8) {
		return 0;
	}

	BYTE* file_in = (BYTE*)file_inout;
	DWORD magic = *(DWORD*)file_in;
	if (magic != 4) {
		return 0;
	}

	char path[MAX_PATH];
	strcpy(path, fn);
	char *path_fn = strrchr(path, '/') + 1;

	DWORD nb_files = *(DWORD*)(file_in + 4);
	file_in += 8; size_in -= 8;

	bool file_changed = false;
	for (DWORD i = 0; i < nb_files; i++) {
		if (size_in < 4) {
			return 0;
		}
		DWORD fn_size = *(DWORD*)file_in;
		file_in += 4; size_in -= 4;
		if (size_in < fn_size + sizeof(DatDesc)) {
			return 0;
		}

		memcpy(path_fn, file_in, fn_size);
		path_fn[fn_size] = '\0';
		file_in += fn_size; size_in -= fn_size;

		// Retrieve the PNG width/height
		strcpy(PathFindExtensionA(path_fn), ".png");
		uint32_t width, height;
		bool IHDR_read = png_image_get_IHDR(path, &width, &height, nullptr);
		if (!IHDR_read) {
			file_in += sizeof(DatDesc); size_in -= sizeof(DatDesc);
			continue;
		}

		DatDesc *desc = (DatDesc*)file_in;
		if (desc->w != width && desc->h != height) {
			desc->x = 0;
			desc->y = 0;
			desc->w = width;
			desc->h = height;
			file_changed = true;
		}
		file_in += sizeof(DatDesc); size_in -= sizeof(DatDesc);
	}
	return file_changed;
}

size_t get_bmp_size(const char *fn, json_t*, size_t)
{
	return sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + get_image_data_size(fn, false);
}

int patch_bmp(void *file_inout, size_t size_out, size_t, const char *fn, json_t*)
{
	STRDUP_VLA(char, fn_png, fn);
	strcpy(PathFindExtensionA(fn_png), ".png");

	uint32_t width, height, rowbytes;
	uint8_t bpp;
	BYTE** row_pointers = png_image_read(fn_png, &width, &height, &bpp);
	VLA_FREE(fn_png);

	if (!row_pointers) {
		return 0;
	}

	rowbytes = width * bpp / 8;
	if (rowbytes % 4 != 0) {
		rowbytes += 4 - (rowbytes % 4);
	}
	if (size_out < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + rowbytes * height) {
		log_print("Destination buffer too small!\n");
		free(row_pointers);
		return -1;
	}

	BITMAPFILEHEADER *bpFile = (BITMAPFILEHEADER*)file_inout;
	BITMAPINFOHEADER *bpInfo = (BITMAPINFOHEADER*)(bpFile + 1);
	BYTE *bpData = (BYTE*)(bpInfo + 1);
	bpFile->bfType = 0x4D42;
	bpFile->bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + rowbytes * height;
	bpFile->bfReserved1 = 0;
	bpFile->bfReserved2 = 0;
	bpFile->bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	bpInfo->biSize = sizeof(BITMAPINFOHEADER);
	bpInfo->biWidth = width;
	bpInfo->biHeight = height;
	bpInfo->biPlanes = 1;
	bpInfo->biBitCount = bpp;
	bpInfo->biCompression = (bpp == 32 ? BI_BITFIELDS : BI_RGB);
	bpInfo->biSizeImage = rowbytes * height;
	bpInfo->biXPelsPerMeter = 65535;
	bpInfo->biYPelsPerMeter = 65535;
	bpInfo->biClrUsed = 0;
	bpInfo->biClrImportant = 0;

	for (size_t h = 0; h < height; h++) {
		size_t pos = 0;
		for (size_t w = 0; w < width; w++) {
			bpData[h * rowbytes + pos + 0] = row_pointers[height - h - 1][w * bpp / 8 + 2];
			bpData[h * rowbytes + pos + 1] = row_pointers[height - h - 1][w * bpp / 8 + 1];
			bpData[h * rowbytes + pos + 2] = row_pointers[height - h - 1][w * bpp / 8 + 0];
			if (bpp == 32) {
				bpData[h * rowbytes + pos + 3] = row_pointers[height - h - 1][w * bpp / 8 + 3];
			}
			pos += bpp / 8;
		}
		// Padding
		while (pos % 4) {
			bpData[h * rowbytes + pos] = 0;
			pos++;
		}
	}

	free(row_pointers);
	return 1;
}
