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

size_t get_cv2_size(const char *fn, json_t*, size_t);
size_t get_cv2_size_for_th123(const char *fn, json_t*, size_t);
size_t get_bmp_size(const char *fn, json_t*, size_t);
int patch_cv2(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t*);
int patch_cv2_for_th123(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t*);
int patch_bmp(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t*);

static CRITICAL_SECTION cs;

int nsml_init()
{
	if (game_id == TH_MEGAMARI) {
		patchhook_register("*.bmp", patch_bmp, get_bmp_size);
	}
	else if (game_id == TH_NSML) {
		// Increasing the file size makes the game crash.
		patchhook_register("*.cv2", patch_cv2, nullptr);
	}
	else if (game_id == TH105) {
		patchhook_register("*.cv2", patch_cv2, get_cv2_size);
	}
	else if (game_id == TH123) {
		patchhook_register("*.cv2", patch_cv2_for_th123, get_cv2_size_for_th123);
	}
	return 0;
}

/**
  * Replace the game_id and build with fake ones.
  * To remove the game_id or the build, pass a pointer to NULL.
  * When the function returns, game_id and build contains the old game_id and build.
  * To restore the values, call the function again with the same parameters.
  */
static void change_game_id(json_t **game_id, json_t **build)
{
	json_t *old_game_id = json_object_get(runconfig_get(), "game");
	json_t *old_build = json_object_get(runconfig_get(), "build");
	json_incref(old_game_id);
	json_incref(old_build);

	if (*game_id) {
		json_object_set(runconfig_get(), "game", *game_id);
		json_decref(*game_id);
	}
	else {
		json_object_del(runconfig_get(), "game");
	}
	if (*build) {
		json_object_set(runconfig_get(), "build", *build);
		json_decref(*build);
	}
	else {
		json_object_del(runconfig_get(), "build");
	}

	*game_id = old_game_id;
	*build = old_build;
}

template<typename T>
static void run_with_game_fallback(const char *game_id, const char *build, T func)
{
	json_t *j_game_id = nullptr;
	json_t *j_build = nullptr;
	if (game_id) {
		j_game_id = json_string(game_id);
	}
	if (build) {
		j_build = json_string(build);
	}
	EnterCriticalSection(&cs);
	change_game_id(&j_game_id, &j_build);
	func();
	change_game_id(&j_game_id, &j_build);
	LeaveCriticalSection(&cs);
}

size_t get_image_data_size(const char *fn, bool fill_alpha_for_24bpp)
{
	VLA(char, fn_png, strlen(fn) + 1);
	strcpy(fn_png, fn);
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

size_t get_cv2_size_for_th123(const char *fn, json_t*, size_t)
{
	size_t size = get_image_data_size(fn, true);
	if (size == 0) {
		run_with_game_fallback("th105", nullptr, [fn, &size]() {
			size = get_image_data_size(fn, true);
		});
	}
	return 17 + size;
}

int patch_cv2_for_th123(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
	int ret = patch_cv2(file_inout, size_out, size_in, fn, patch);
	if (ret <= 0) {
		run_with_game_fallback("th105", nullptr, [file_inout, size_out, size_in, fn, patch, &ret]() {
			ret = patch_cv2(file_inout, size_out, size_in, fn, patch);
		});
	}
	return ret;
}

size_t get_bmp_size(const char *fn, json_t*, size_t)
{
	return sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + get_image_data_size(fn, false);
}

int patch_bmp(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t*)
{
	VLA(char, fn_png, strlen(fn) + 1);
	strcpy(fn_png, fn);
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

	for (unsigned int h = 0; h < height; h++) {
		unsigned int pos = 0;
		for (unsigned int w = 0; w < width; w++) {
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

int BP_nsml_file_header(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	const char *filename = (const char*)json_object_get_immediate(bp_info, regs, "file_name");
	size_t fn_size = json_object_get_immediate(bp_info, regs, "fn_size");
	json_t *game_fallback = json_object_get(bp_info, "game_fallback");
	// ----------

	char *uFilename;
	if (fn_size) {
		uFilename = EnsureUTF8(filename, fn_size);
	}
	else {
		uFilename = EnsureUTF8(filename, strlen(filename));
	}
	CharLowerA(uFilename);

	json_t *new_bp_info = json_copy(bp_info);
	json_object_set_new(new_bp_info, "file_name", json_integer((json_int_t)uFilename));
	int ret = BP_file_header(regs, new_bp_info);

	if (game_fallback) {
		// If no file was found, try again with the files from another game.
		// Used for th123, that loads the file from th105.
		file_rep_t *fr = file_rep_get(filename);
		if (!fr || (!fr->rep_buffer && !fr->patch)) {
			run_with_game_fallback(json_string_value(game_fallback), nullptr, [regs, new_bp_info, &ret]() {
				ret = BP_file_header(regs, new_bp_info);
			});
		}
	}

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
	const char *file_name = (const char*)json_object_get_immediate(bp_info, regs, "file_name");
	// ----------

	json_t *new_bp_info = json_copy(bp_info);
	char *uFilename = nullptr;

	if (file_name) {
		uFilename = EnsureUTF8(file_name, strlen(file_name));
		CharLowerA(uFilename);
		json_object_set_new(new_bp_info, "file_name", json_integer((json_int_t)uFilename));
	}

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

// In th105, relying on the last open file doesn't work. So we'll use the file object instead.
int BP_th105_file_new(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	const char *file_name = (const char*)json_object_get_immediate(bp_info, regs, "file_name");
	void *file_object = (void*)json_object_get_immediate(bp_info, regs, "file_object");
	// ----------

	if (!file_name || !file_object) {
		return 1;
	}
	char *uFilename = EnsureUTF8(file_name, strlen(file_name));
	CharLowerA(uFilename);
	file_rep_t *fr = file_rep_get(uFilename);
	if (fr) {
		file_rep_set_object(fr, file_object);
	}
	free(uFilename);
	return 1;
}

int BP_th105_file_delete(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	void *file_object = (void*)json_object_get_immediate(bp_info, regs, "file_object");
	// ----------

	if (!file_object) {
		return 1;
	}
	file_rep_t *fr = file_rep_get_by_object(file_object);
	if (fr) {
		file_rep_set_object(fr, nullptr);
	}
	return 1;
}

extern "C" int nsml_mod_init()
{
	InitializeCriticalSection(&cs);
	return 0;
}

extern "C" void nsml_mod_exit()
{
	DeleteCriticalSection(&cs);
}
