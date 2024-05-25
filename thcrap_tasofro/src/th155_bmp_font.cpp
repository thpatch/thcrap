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
#include "th155_bmp_font.h"
#include <libs/135tk/bmpfont/bmpfont_create.h>

// Increment this number to force a bmpfont cache refresh
static const int cache_version = 2;

size_t get_bmp_font_size(const char *fn, json_t *patch, size_t)
{
	if (patch) {
		// TODO: generate and cache the file here, return the true size.
		return 64 * 1024 * 1024;
	}

	size_t size = 0;

	size_t fn_len = strlen(fn);
	VLA(char, fn_buf, fn_len + 5);
	memcpy(fn_buf, fn, fn_len);
	memcpy(fn_buf + fn_len, ".png", sizeof(".png"));

	uint32_t width, height;
	bool IHDR_read = png_image_get_IHDR(fn_buf, &width, &height, nullptr);
	if (IHDR_read) {
		size += sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + width * height;
	}

	memcpy(fn_buf + fn_len, ".bin", sizeof(".bin"));

	size_t bin_size;
	void *bin_data = stack_game_file_resolve(fn_buf, &bin_size);
	if (bin_data) {
		free(bin_data);
		size += bin_size;
	}

	VLA_FREE(fn_buf);

	return size;
}

void add_json_file(char *chars_list, int& chars_list_count, json_t *file)
{
	if (json_is_object(file)) {
		json_t *it;
		json_object_foreach_value(file, it) {
			json_incref(it);
			add_json_file(chars_list, chars_list_count, it);
		}
	}
	else if (json_is_array(file)) {
		json_t *it;
		json_array_foreach_scoped(size_t, i, file, it) {
			json_incref(it);
			add_json_file(chars_list, chars_list_count, it);
		}
	}
	else if (json_is_string(file)) {
		const char *str = json_string_value(file);
		WCHAR_T_DEC(str);
		WCHAR_T_CONV(str);
		for (int i = 0; str_w[i]; i++) {
			if (!chars_list[str_w[i]]) {
				chars_list[str_w[i]] = 1;
				chars_list_count++;
			}
		}
		WCHAR_T_FREE(str);
	}
	json_decref(file);
}

static void add_json_file(char *chars_list, int& chars_list_count, const std::string& path)
{
	size_t file_size;
	char *file_buffer = (char*)file_read(path.c_str(), &file_size);
	if (!file_buffer) {
		return;
	}

	add_json_file(chars_list, chars_list_count, json_loadb(file_buffer, file_size, 0, nullptr));
	free(file_buffer);
}

static void add_files_in_directory(char *chars_list, int& chars_list_count, std::string basedir, bool recurse)
{
	std::string pattern = basedir + "\\*";
	WIN32_FIND_DATAA ffd;
	HANDLE hFind = FindFirstFile(pattern.c_str(), &ffd);
	if (hFind == INVALID_HANDLE_VALUE) {
		return;
	}

	do
	{
		if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) {
			// Do nothing
		}
		else if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (recurse) {
				std::string dirname = basedir + "\\" + ffd.cFileName;
				add_files_in_directory(chars_list, chars_list_count, dirname, recurse);
			}
		}
		else if (strcmp(PathFindExtensionA(ffd.cFileName), ".js") == 0 ||
				 strcmp(PathFindExtensionA(ffd.cFileName), ".jdiff") == 0) {
			std::string filename = basedir + "\\" + ffd.cFileName;
			log_printf(" + %s\n", filename.c_str());
			add_json_file(chars_list, chars_list_count, filename);
		}
	} while (FindNextFile(hFind, &ffd));

	FindClose(hFind);
}

// Add the characters of the original font - we will need them if we display unpatched text.
int fill_chars_list_from_font(char *chars_list, const void *file_inout, size_t size_in)
{
	struct Metadata
	{
		uint16_t unk;
		uint16_t nb_chars;
	};
	static_assert(sizeof(Metadata) == 4);

	if (size_in < sizeof(BITMAPFILEHEADER)) {
		return 0;
	}
	const BITMAPFILEHEADER *bpFile = (const BITMAPFILEHEADER*)file_inout;

	if (size_in < bpFile->bfSize + sizeof(Metadata)) {
		return 0;
	}
	const Metadata *metadata = (const Metadata*)((const BYTE*)file_inout + bpFile->bfSize);

	if (size_in < bpFile->bfSize + sizeof(Metadata) + (metadata->nb_chars * 2)) {
		return 0;
	}
	char *chars = (char*)(metadata + 1);
	int chars_list_count = 0;

	for (int i = 0; i < metadata->nb_chars; i++) {
		char cstr[2];
		WCHAR wstr[3];
		if (chars[i * 2 + 1] != 0)
		{
			cstr[0] = chars[i * 2 + 1];
			cstr[1] = chars[i * 2];
		}
		else
		{
			cstr[0] = chars[i * 2];
			cstr[1] = 0;
		}
		MultiByteToWideChar(932, 0, cstr, 2, wstr, 3);

		if (!chars_list[wstr[0]]) {
			chars_list[wstr[0]] = 1;
			chars_list_count++;
		}
	}

	return chars_list_count;
}

// Add all the characters that we could possibly want to display.
int fill_chars_list_from_files(char *chars_list, json_t *files)
{
	int chars_list_count = 0;

	json_t *it;
	json_flex_array_foreach_scoped(size_t, i, files, it) {
		const char *fn = json_string_value(it);
		if (!fn) {
			// Do nothing
		}
		else if (strcmp(fn, "*") == 0) {
			log_print("(Font) Searching in every js file for characters...\n");
			stack_foreach_cpp([&](const patch_t *patch) {
				if (patch->archive) {
					add_files_in_directory(chars_list, chars_list_count, patch->archive, false);
					const char *game = runconfig_game_get();
					if (game) {
						add_files_in_directory(chars_list, chars_list_count, std::string(patch->archive) + "\\" + game, true);
					}
				}
			});
		}
		else {
			add_json_file(chars_list, chars_list_count, stack_json_resolve(fn, nullptr));
		}
	}

	return chars_list_count;
}

// Returns the number of elements set to true in the list
int fill_chars_list(char *chars_list, void *file_inout, size_t size_in, json_t *files)
{
	int chars_list_count  = fill_chars_list_from_font(chars_list, file_inout, size_in);
	chars_list_count     += fill_chars_list_from_files(chars_list, files);
	return chars_list_count;
}

void bmpfont_update_cache(std::string fn, char *chars_list, int chars_count, BYTE *buffer, size_t buffer_size, json_t *patch)
{
	json_t *cache_patch = json_object();
	json_object_set_new(cache_patch, "cache_version", json_integer(cache_version));
	json_object_set(cache_patch, "patch", patch);
	json_object_set_new(cache_patch, "chars_count", json_integer(chars_count));
	json_t *chars_list_json = json_array();
	for (WCHAR i = 0; i < 65535; i++) {
		json_array_append_new(chars_list_json, json_boolean(chars_list[i]));
	}
	json_object_set(cache_patch, "chars_list", chars_list_json);
	chars_list_json = json_decref_safe(chars_list_json);

	std::string full_path;
	{
		std::string fn_jdiff = fn + ".jdiff";
		char **chain = resolve_chain_game(fn_jdiff.c_str());
		char *c_full_path = stack_fn_resolve_chain(chain);
		chain_free(chain);
		full_path.assign(c_full_path, strlen(c_full_path) - strlen(".jdiff")); // Remove ".jdiff"
		free(c_full_path);
	}

	full_path += ".cache";
	file_write(full_path.c_str(), buffer, buffer_size);

	full_path += ".jdiff";
	FILE *fp = fopen_u(full_path.c_str(), "w");
	json_dumpf(cache_patch, fp, JSON_INDENT(2));
	fclose(fp);

	json_decref(cache_patch);
}

BYTE *read_bmpfont_from_cache(std::string fn, char *chars_list, int chars_count, json_t *patch, size_t *file_size)
{
	std::string cache_patch_fn = fn + ".cache.jdiff";
	json_t *cache_patch = stack_game_json_resolve(cache_patch_fn.c_str(), nullptr);
	if (!cache_patch) {
		return nullptr;
	}

	if (json_equal(patch, json_object_get(cache_patch, "patch")) == 0) {
		json_decref(cache_patch);
		return nullptr;
	}

	if (chars_count   != json_integer_value(json_object_get(cache_patch, "chars_count")) ||
		cache_version != json_integer_value(json_object_get(cache_patch, "cache_version"))) {
		json_decref(cache_patch);
		return nullptr;
	}
	json_t *chars_list_cache = json_object_get(cache_patch, "chars_list");
	for (WCHAR i = 0; i < 65535; i++) {
		if (chars_list[i] != json_boolean_value(json_array_get(chars_list_cache, i))) {
			json_decref(chars_list_cache);
			json_decref(cache_patch);
			return nullptr;
		}
	}
	json_decref(chars_list_cache);
	json_decref(cache_patch);

	std::string cache_fn = fn + ".cache";
	return (BYTE*)stack_game_file_resolve(cache_fn.c_str(), file_size);
}

int bmpfont_add_option_int(void *bmpfont, const char *name, int value)
{
	char s_value[11];
	sprintf(s_value, "%d", value);
	return bmpfont_add_option(bmpfont, name, s_value);
}

int bmpfont_add_option_color(void *bmpfont, const char *name, json_t *value)
{
	char s_value[10 * 3 + 3];
	sprintf(s_value, "%lld:%lld:%lld",
		json_integer_value(json_array_get(value, 0)),
		json_integer_value(json_array_get(value, 1)),
		json_integer_value(json_array_get(value, 2)));
	return bmpfont_add_option(bmpfont, name, s_value);
}

bool generate_bitmap_font(void *bmpfont, char *chars_list, json_t *patch, BYTE **buffer, size_t *buffer_size)
{
	if (bmpfont == nullptr) {
		return false;
	}

	void *font_file = nullptr;
	int ret = 1;

	// Code page
	ret &= bmpfont_add_option_int(bmpfont, "--cp", CP_UTF8);

	// Plugin
	const char *thcrap_dir = runconfig_thcrap_dir_get();
	const char *plugin     = json_object_get_string(patch, "plugin");
	if (plugin == nullptr) {
		log_print("[Bmpfont] Jdiff file must have a 'plugin' entry\n");
		return false;
	}
	std::string plugin_path = thcrap_dir;
	plugin_path += "\\bin\\";
	plugin_path += plugin;
	plugin_path += DEBUG_OR_RELEASE ".dll";
	ret &= bmpfont_add_option(bmpfont, "--plugin", plugin_path.c_str());

	// Json options
	json_t *json_value;
	if ((json_value = json_object_get(patch, "font_size"))) {
		ret &= bmpfont_add_option_int(bmpfont, "--font-size", (int)json_integer_value(json_value));
	}
	if ((json_value = json_object_get(patch, "outline_width"))) {
		ret &= bmpfont_add_option_int(bmpfont, "--outline-width", (int)json_integer_value(json_value));
	}
	if ((json_value = json_object_get(patch, "margin"))) {
		ret &= bmpfont_add_option_int(bmpfont, "--margin", (int)json_integer_value(json_value));
	}
	if ((json_value = json_object_get(patch, "rotate"))) {
		ret &= bmpfont_add_option_int(bmpfont, "--rotate", (int)json_integer_value(json_value));
	}
	if (json_is_true(json_object_get(patch, "is_bold"))) {
		ret &= bmpfont_add_option(bmpfont, "--bold", "true");
	}
	if ((json_value = json_object_get(patch, "bg_color"))) {
		ret &= bmpfont_add_option_color(bmpfont, "--bg-color", json_value);
	}
	if ((json_value = json_object_get(patch, "outline_color"))) {
		ret &= bmpfont_add_option_color(bmpfont, "--outline-color", json_value);
	}
	if ((json_value = json_object_get(patch, "fg_color"))) {
		ret &= bmpfont_add_option_color(bmpfont, "--text-color", json_value);
	}

	// Font
	char **chain = resolve_chain(json_string_value(json_object_get(patch, "font_file")));
	if (chain && chain[0]) {
		size_t font_file_size;
		log_printf("(Data) Resolving %s... ", chain[0]);
		font_file = file_stream_read(stack_file_resolve_chain(chain), &font_file_size);
		ret &= bmpfont_add_option_binary(bmpfont, "--font-memory", font_file, font_file_size);
	}
	chain_free(chain);
	if ((json_value = json_object_get(patch, "font_name"))) {
		ret &= bmpfont_add_option(bmpfont, "--font-name", json_string_value(json_value));
	}

	ret &= bmpfont_add_option(       bmpfont, "--format", "packed_rgba");
	ret &= bmpfont_add_option_binary(bmpfont, "--out-buffer", buffer, sizeof(BYTE*));
	ret &= bmpfont_add_option_binary(bmpfont, "--out-size", buffer_size, sizeof(size_t));
	ret &= bmpfont_add_option_binary(bmpfont, "--chars-list", chars_list, 65536 * sizeof(char));

	if (!ret) {
		return false;
	}

	ret = bmpfont_run(bmpfont);
	free(font_file);

	if (!ret) {
		return false;
	}
	return true;
}

int patch_bmp_font(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
	if (patch) {
		// List all the characters to include in out font
		char *chars_list = (char*)malloc(sizeof(char[65536]));
		for (size_t i = 0; i < 65536; i++) {
			chars_list[i] = 0;
		}
		int chars_count = 0;
		chars_count = fill_chars_list(chars_list, file_inout, size_in, json_object_get(patch, "chars_source"));

		// Create the bitmap font
		size_t buffer_size;
		BYTE *buffer = read_bmpfont_from_cache(fn, chars_list, chars_count, patch, &buffer_size);
		void *bmpfont = nullptr;
		if (buffer == nullptr) {
			bmpfont = bmpfont_init();
			if (generate_bitmap_font(bmpfont, chars_list, patch, &buffer, &buffer_size)) {
				bmpfont_update_cache(fn, chars_list, chars_count, buffer, buffer_size, patch);
			}
		}
		free(chars_list);
		if (!buffer) {
			log_print("Bitmap font creation failed\n");
			bmpfont_free(bmpfont);
			return -1;
		}
		if (buffer_size > size_out) {
			log_print("Bitmap font too big\n");
			if (bmpfont) {
				bmpfont_free(bmpfont);
			}
			else {
				free(buffer);
			}
			return -1;
		}

		memcpy(file_inout, buffer, buffer_size);
		if (bmpfont) {
			bmpfont_free(bmpfont);
		}
		else {
			free(buffer);
		}
		return 1;
	}

	BITMAPFILEHEADER *bpFile = (BITMAPFILEHEADER*)file_inout;
	BITMAPINFOHEADER *bpInfo = (BITMAPINFOHEADER*)(bpFile + 1);
	BYTE *bpData = (BYTE*)(bpInfo + 1);

	size_t fn_len = strlen(fn);
	VLA(char, fn_buf, fn_len + 5);
	memcpy(fn_buf, fn, fn_len);
	memcpy(fn_buf + fn_len, ".png", sizeof(".png"));

	uint32_t width, height;
	uint8_t bpp;
	BYTE** row_pointers = png_image_read(fn_buf, &width, &height, &bpp, false);

	if (row_pointers) {
		if (!row_pointers || size_out < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + width * height) {
			VLA_FREE(fn_buf);
			log_print("Destination buffer too small!\n");
			free(row_pointers);
			return -1;
		}

		bpFile->bfType = 0x4D42;
		bpFile->bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + width * height;
		bpFile->bfReserved1 = 0;
		bpFile->bfReserved2 = 0;
		bpFile->bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

		bpInfo->biSize = sizeof(BITMAPINFOHEADER);
		bpInfo->biWidth = width / 4;
		bpInfo->biHeight = height;
		bpInfo->biPlanes = 1;
		bpInfo->biBitCount = 32;
		bpInfo->biCompression = BI_RGB;
		bpInfo->biSizeImage = 0;
		bpInfo->biXPelsPerMeter = 0;
		bpInfo->biYPelsPerMeter = 0;
		bpInfo->biClrUsed = 0;
		bpInfo->biClrImportant = 0;

		for (unsigned int h = 0; h < height; h++) {
			for (unsigned int w = 0; w < width; w++) {
				bpData[h * width + w] = row_pointers[height - h - 1][w];
			}
		}
		free(row_pointers);
	}

	memcpy(fn_buf + fn_len, ".bin", sizeof(".bin"));

	size_t bin_size;
	void *bin_data = stack_game_file_resolve(fn_buf, &bin_size);
	VLA_FREE(fn_buf);

	if (bin_data) {
		if (size_out < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bpInfo->biWidth * 4 * bpInfo->biHeight + bin_size) {
			log_print("Destination buffer too small!\n");
			free(bin_data);
			return -1;
		}

		void *metadata = (BYTE*)file_inout + bpFile->bfSize;
		memcpy(metadata, bin_data, bin_size);

		free(bin_data);
	}
	return 1;
}

extern "C" int BP_bmpfont_fix_parameters(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	const char *string = (const char*)json_object_get_immediate(bp_info, regs, "string");
	size_t string_location = json_object_get_immediate(bp_info, regs, "string_location");
	int *offset = (int*)json_object_get_pointer(bp_info, regs, "offset");
	size_t *c1 = json_object_get_pointer(bp_info, regs, "c1");
	size_t *c2 = json_object_get_pointer(bp_info, regs, "c2");
	// ----------

	if (string_location >= 0x10) {
		string = *(const char**)string;
	}
	string += *offset;

	wchar_t wc[2];
	size_t codepoint_len = CharNextU(string) - string;
	MultiByteToWideCharU(0, 0, string, codepoint_len, wc, 2);
	*c1 = wc[0] >> 8;
	*c2 = wc[0] & 0xFF;

	*offset += codepoint_len - 1;

	return 1;
}
