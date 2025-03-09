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
static const size_t cache_version = 3;

static constexpr size_t MAX_CHARS = 65536;
static constexpr size_t PACKED_CHARS = MAX_CHARS / 4;

size_t get_bmp_font_size(const char *fn, json_t *patch, size_t)
{
	if (patch) {
		// TODO: generate and cache the file here, return the true size.
		return 64 * 1024 * 1024;
	}

	size_t fn_len = strlen(fn);
	BUILD_VLA_STR(char, fn_buf, fn, fn_len, ".png");

	uint32_t width, height;
	bool IHDR_read = png_image_get_IHDR(fn_buf, &width, &height, nullptr);

	size_t size = 0;
	if (IHDR_read) {
		size += sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + width * height;
	}

	*(uint32_t*)&fn_buf[fn_len] = TextInt('.', 'b', 'i', 'n');

	HANDLE stream = stack_game_file_stream(fn_buf);
	if (stream != INVALID_HANDLE_VALUE) {
		size += file_stream_size(stream);
		CloseHandle(stream);
	}

	VLA_FREE(fn_buf);

	return size;
}

void add_json_file(char *TH_RESTRICT chars_list, json_t *file)
{
	if (json_is_object(file)) {
		json_t *it;
		json_object_foreach_value(file, it) {
			add_json_file(chars_list, it);
		}
	}
	else if (json_is_array(file)) {
		json_t *it;
		json_array_foreach_scoped(size_t, i, file, it) {
			add_json_file(chars_list, it);
		}
	}
	else if (json_is_string(file)) {
		const char *str = json_string_value(file);
		size_t str_len = json_string_length(file) + 1;
		VLA(wchar_t, str_w, str_len);
		int convert_count = StringToUTF16(str_w, str, str_len) - 1;
		if (convert_count > 0) {
			wchar_t* str_w_read = str_w;
			size_t wchar_count = convert_count;
			do {
				uint32_t c = *str_w_read++;
				chars_list[c] = true;
			} while (--wchar_count);
		}
		VLA_FREE(str_w);
	}
}

static void add_json_file(char *TH_RESTRICT chars_list, const char* path)
{
	size_t file_size;
	char *file_buffer = (char*)file_read(path, &file_size);
	if (file_buffer) {
		json_t* file = json_loadb(file_buffer, file_size, 0, nullptr);
		free(file_buffer);
		add_json_file(chars_list, file);
		json_decref_fast(file);
	}
}

static void add_files_in_directory(char *TH_RESTRICT chars_list, const std::string& basedir, bool recurse)
{
	std::string pattern = basedir + "\\*";
	WIN32_FIND_DATAA ffd;
	HANDLE hFind = FindFirstFileU(pattern.c_str(), &ffd);

	if (hFind != INVALID_HANDLE_VALUE) {

		do {
			// These sorts of integer comparisons are safe here because
			// the buffer is known to be part of a larger struct
			if (*(uint16_t*)ffd.cFileName == TextInt('.', '\0') ||
				(*(uint32_t*)ffd.cFileName & 0xFFFFFF) == TextInt('.', '.', '\0')
			) {
				// Do nothing
			}
			else if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if (recurse) {
					add_files_in_directory(chars_list, basedir + '\\' + ffd.cFileName, true);
				}
			}
			else {
				char* extension = PathFindExtensionA(ffd.cFileName);
				if (*(uint32_t*)extension == TextInt('.', 'j', 's', '\0') ||
					*(uint32_t*)extension == TextInt('.', 'j', 'd', 'i') && (*(uint32_t*)(extension + 4) & 0xFFFFFF) == TextInt('f', 'f', '\0')
				) {
					std::string filename = basedir + '\\' + ffd.cFileName;
					log_printf(" + %s\n", filename.c_str());
					add_json_file(chars_list, filename.c_str());
				}
			}
		} while (FindNextFileU(hFind, &ffd));

		FindClose(hFind);
	}
}

// Add the characters of the original font - we will need them if we display unpatched text.
void fill_chars_list_from_font(char *TH_RESTRICT chars_list, const void *file_inout, size_t size_in)
{
	struct Metadata
	{
		uint16_t unk;
		uint16_t nb_chars;
	};
	static_assert(sizeof(Metadata) == 4);

	if unexpected(size_in < sizeof(BITMAPFILEHEADER)) {
		return ;
	}
	const BITMAPFILEHEADER *bpFile = (const BITMAPFILEHEADER*)file_inout;

	if unexpected(size_in < bpFile->bfSize + sizeof(Metadata)) {
		return ;
	}
	const Metadata *metadata = (const Metadata*)((const BYTE*)file_inout + bpFile->bfSize);

	if unexpected(size_in < bpFile->bfSize + sizeof(Metadata) + (metadata->nb_chars * 2)) {
		return ;
	}

	const uint16_t *chars = (const uint16_t*)(metadata + 1);

	if (size_t i = metadata->nb_chars) {

		do {
			uint32_t temp = *chars++;
			if (temp > UINT8_MAX) {
				temp = _rotl16(temp, 8);
			}
			uint32_t cstr = temp;

			WCHAR wstr[3];
			MultiByteToWideChar(932, 0, (LPCSTR)&cstr, 2, wstr, 3);

			uint32_t c = wstr[0];
			chars_list[c] = true;
		} while (--i);
	}
}

alignas(16) static char ALL_FILES_CHARS_LIST[MAX_CHARS] = {};
static bool all_files_list_initialized = false;

// This was previously a lambda, but MSVC is stupid
// when giving lambdas calling conventions
void TH_CDECL fill_chars_list_from_all_files_impl(const patch_t* patch, void*) {
	std::string path = patch->archive;
	add_files_in_directory(ALL_FILES_CHARS_LIST, path, false);
	std::string_view game = runconfig_game_get_view();
	if (!game.empty()) {
		add_files_in_directory(ALL_FILES_CHARS_LIST, (path += '\\') += game, true);
	}
}

void fill_chars_list_from_all_files(char *TH_RESTRICT chars_list) {
	if unexpected(!all_files_list_initialized) {
		log_print("(Font) Searching in every js file for characters...\n");
		stack_foreach(fill_chars_list_from_all_files_impl, NULL);
		all_files_list_initialized = true;
	}
	for (size_t i = 0; i < MAX_CHARS; ++i) {
		chars_list[i] |= ALL_FILES_CHARS_LIST[i];
	}
}

// In case this ever needs to be repatch compatible, uncomment this.
// Currently the code only ever gets run during startup.
/*
extern "C" TH_EXPORT void bmpfont_mod_repatch(json_t* files_changed) {
	const char *fn;
	json_object_foreach_key(files_changed, fn) {
		char* extension = PathFindExtensionA(fn);
		if (strcmp(extension, ".js") == 0 ||
			strcmp(extension, ".jdiff") == 0
		) {
			all_files_list_initialized = false;
			break;
		}
	}
}
*/

// Add all the characters that we could possibly want to display.
void fill_chars_list_from_files(char *TH_RESTRICT chars_list, json_t *files)
{
	json_t *it;
	json_flex_array_foreach_scoped(size_t, i, files, it) {
		const char *fn = json_string_value(it);
		if (!fn) {
			// Do nothing
		}
		else if (fn[0] == '*' && fn[1] == '\0') {
			fill_chars_list_from_all_files(chars_list);
		}
		else {
			json_t* json_file = stack_json_resolve(fn, nullptr);
			add_json_file(chars_list, json_file);
			json_decref_fast(json_file);
		}
	}
}

// Returns the number of elements set to true in the list
void fill_chars_list(char *TH_RESTRICT chars_list, void *file_inout, size_t size_in, json_t *files)
{
	fill_chars_list_from_font(chars_list, file_inout, size_in);
	fill_chars_list_from_files(chars_list, files);
}

void bmpfont_update_cache(const char* fn, char *TH_RESTRICT chars_list, size_t chars_count, BYTE *buffer, size_t buffer_size, json_t *patch)
{
	json_t *cache_patch = json_object();
	json_object_set_new(cache_patch, "cache_version", json_integer(cache_version));
	json_object_set(cache_patch, "patch", patch);
	json_object_set_new(cache_patch, "chars_count", json_integer(chars_count));

	// Convert chars_list to a string of packed bits
	__m128i text_conv = _mm_set_epi8('0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0');
	for (size_t i = 0; i < PACKED_CHARS; i += 16) {
		__m128i A = _mm_loadu_si128((__m128i*)(chars_list + i * 4));
		__m128i B = _mm_loadu_si128((__m128i*)(chars_list + i * 4 + 16));
		B = _mm_add_epi8(B, B);
		B = _mm_add_epi8(B, A);
		A = _mm_loadu_si128((__m128i*)(chars_list + i * 4 + 32));
		A = _mm_slli_epi16(A, 2);
		A = _mm_add_epi8(A, B);
		B = _mm_loadu_si128((__m128i*)(chars_list + i * 4 + 48));
		B = _mm_slli_epi16(B, 3);
		B = _mm_add_epi8(B, A);
		B = _mm_add_epi8(B, text_conv);
		_mm_storeu_si128((__m128i*)(chars_list + i), B);
	}
	json_object_set_new(cache_patch, "chars_list", json_stringn_nocheck(chars_list, PACKED_CHARS));
	

	std::string path = fn;
	path += ".jdiff";

	char **chain = resolve_chain_game(path.c_str());
	char *c_full_path = stack_fn_resolve_chain(chain);
	chain_free(chain);
	path.assign(c_full_path, strlen(c_full_path) - strlen("jdiff")); // Remove "jdiff"
	free(c_full_path);

	path += "cache";
	file_write(path.c_str(), buffer, buffer_size);

	path += 'j';
	FILE *fp = fopen_u(path.c_str(), "wb");
	json_dumpf(cache_patch, fp, JSON_INDENT(2) | JSON_COMPACT);
	fclose(fp);

	json_decref_fast(cache_patch);
}

BYTE *read_bmpfont_from_cache(const char* fn, char *TH_RESTRICT chars_list, size_t chars_count, json_t *patch, size_t *file_size)
{
	size_t fn_len = strlen(fn);
	BUILD_VLA_STR(char, fn_buf, fn, fn_len, ".cachej");

	json_t *cache_patch = stack_game_json_resolve(fn_buf, NULL);
	if (cache_patch) {
		if (cache_version == json_integer_value(json_object_get(cache_patch, "cache_version")) &&
			chars_count == json_integer_value(json_object_get(cache_patch, "chars_count")) &&
			json_equal(patch, json_object_get(cache_patch, "patch"))
		) {
			json_t *chars_list_cache = json_object_get(cache_patch, "chars_list");
			if (chars_list_cache &&
				json_string_length(chars_list_cache) == PACKED_CHARS
			) {
				const char* cached_chars_list = json_string_value(chars_list_cache);

				// Convert chars_list to a string of packed bits
				// for comparing with the previously saved list
				__m128i text_conv = _mm_set_epi8('0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0');
				for (size_t i = 0; i < PACKED_CHARS; i += 16) {
					__m128i A = _mm_loadu_si128((__m128i*)(chars_list + i * 4));
					__m128i B = _mm_loadu_si128((__m128i*)(chars_list + i * 4 + 16));
					B = _mm_add_epi8(B, B);
					B = _mm_add_epi8(B, A);
					A = _mm_loadu_si128((__m128i*)(chars_list + i * 4 + 32));
					A = _mm_slli_epi16(A, 2);
					A = _mm_add_epi8(A, B);
					B = _mm_loadu_si128((__m128i*)(chars_list + i * 4 + 48));
					B = _mm_slli_epi16(B, 3);
					B = _mm_add_epi8(B, A);
					B = _mm_add_epi8(B, text_conv);
					if unexpected(_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_loadu_si128((__m128i*)(cached_chars_list + i)), B)) != 0xFFFF) {
						goto fail;
					}
				}

				json_decref_fast(cache_patch);

				// Remove the 'j' from the end of the filename
				fn_buf[fn_len + strlen(".cache")] = '\0';
				BYTE* ret = (BYTE*)stack_game_file_resolve(fn_buf, file_size);
				VLA_FREE(fn_buf);
				return ret;
			}
		}
fail:
		json_decref_fast(cache_patch);
	}
	VLA_FREE(fn_buf);
	return nullptr;
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

bool generate_bitmap_font(void *bmpfont, char *TH_RESTRICT chars_list, json_t *patch, BYTE **buffer, size_t *buffer_size)
{
	if unexpected(bmpfont == nullptr) {
		return false;
	}

	void *font_file = nullptr;
	int ret = 1;

	// Code page
	ret &= bmpfont_add_option_int(bmpfont, "--cp", CP_UTF8);

	// Plugin
	const char *thcrap_dir = runconfig_thcrap_dir_get();
	const char *plugin     = json_object_get_string(patch, "plugin");
	if unexpected(plugin == nullptr) {
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
	ret &= bmpfont_add_option_binary(bmpfont, "--chars-list", chars_list, sizeof(char[MAX_CHARS]));

	if unexpected(!ret) {
		return false;
	}

	ret = bmpfont_run(bmpfont);
	free(font_file);

	return (bool)ret;
}

int patch_bmp_font(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
	if (patch) {
		// List all the characters to include in out font
		char *chars_list = (char*)malloc(sizeof(char[MAX_CHARS]));

		memset(chars_list, 0, sizeof(char[MAX_CHARS]));

		fill_chars_list(chars_list, file_inout, size_in, json_object_get(patch, "chars_source"));

		// MSVC vectorizes this, which is more efficient than passing
		// around a count variable while filling the chars_list
		size_t chars_count = 0;
		for (size_t i = 0; i < MAX_CHARS; ++i) {
			chars_count += (uint8_t)chars_list[i];
		}

		// Create the bitmap font
		size_t buffer_size;
		BYTE *buffer = read_bmpfont_from_cache(fn, chars_list, chars_count, patch, &buffer_size);
		void *bmpfont = nullptr;
		if (buffer == nullptr) {
			bmpfont = bmpfont_init();
			if (generate_bitmap_font(bmpfont, chars_list, patch, &buffer, &buffer_size)) {
				// chars list is no longer valid after this function call
				// since it gets modified to a text based format
				bmpfont_update_cache(fn, chars_list, chars_count, buffer, buffer_size, patch);
			}
		}
		free(chars_list);
		if unexpected(!buffer) {
			log_print("Bitmap font creation failed\n");
			bmpfont_free(bmpfont);
			return -1;
		}
		if unexpected(buffer_size > size_out) {
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
	BUILD_VLA_STR(char, fn_buf, fn, fn_len, ".png");

	uint32_t width, height;
	uint8_t bpp;
	BYTE** row_pointers = png_image_read(fn_buf, &width, &height, &bpp, false);

	if (row_pointers) {
		if unexpected(!row_pointers || size_out < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + width * height) {
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

		for (size_t h = 0; h < height; h++) {
			for (size_t w = 0; w < width; w++) {
				*bpData++ = row_pointers[height - h - 1][w];
			}
		}
		free(row_pointers);
	}

	*(uint32_t*)&fn_buf[fn_len] = TextInt('.', 'b', 'i', 'n');

	size_t bin_size;
	void *bin_data = stack_game_file_resolve(fn_buf, &bin_size);
	VLA_FREE(fn_buf);

	if (bin_data) {
		if unexpected(size_out < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bpInfo->biWidth * 4 * bpInfo->biHeight + bin_size) {
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

extern "C" size_t BP_bmpfont_fix_parameters(x86_reg_t *regs, json_t *bp_info)
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
