/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * New Super Marisa Land support functions
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"
#include "tfcs.h"
#include "bgm.h"
#include "nsml_images.h"
#include <set>

size_t get_cv2_size_for_th123(const char *fn, json_t*, size_t);
int patch_cv2_for_th123(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t*);
int patch_dat_for_png_for_th123(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch);
static CRITICAL_SECTION cs;
static std::set<const char*, bool(*)(const char*, const char*)> game_fallback_ignore_list([](const char *a, const char *b){ return strcmp(a, b) < 0; });

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
		patchhook_register("*.cv1", patch_csv, nullptr);
		patchhook_register("*.cv2", patch_cv2, get_cv2_size);
		patchhook_register("*.dat", patch_dat_for_png, [](const char*, json_t*, size_t) -> size_t { return 0; });

		char *bgm_fn = fn_for_game("data/csv/system/music.cv1.jdiff");
		jsonvfs_add(bgm_fn, { "themes.js" }, bgm_generator);
		SAFE_FREE(bgm_fn);
	}
	else if (game_id == TH123) {
		patchhook_register("*.cv1", patch_csv, nullptr);
		patchhook_register("*.cv2", patch_cv2_for_th123, get_cv2_size_for_th123);
		patchhook_register("*.dat", patch_dat_for_png_for_th123, [](const char*, json_t*, size_t) -> size_t { return 0; });

		char *bgm_fn           = fn_for_game("data/csv/system/music.cv1.jdiff");
		char *bgm_map_fn       = fn_for_game("data/csv/system/music.cv1.map.jdiff");
		char *bgm_limit_fn     = fn_for_game("data/csv/system/music_limit.cv1.jdiff");
		char *bgm_limit_map_fn = fn_for_game("data/csv/system/music_limit.cv1.map.jdiff");
		jsonvfs_add(bgm_fn, { "themes.js", bgm_map_fn }, bgm_generate_from_map);
		jsonvfs_add(bgm_limit_fn, { "themes.js", bgm_limit_map_fn }, bgm_generate_from_map);
		SAFE_FREE(bgm_fn);
		SAFE_FREE(bgm_map_fn);
		SAFE_FREE(bgm_limit_fn);
		SAFE_FREE(bgm_limit_map_fn);

		json_t *list = stack_game_json_resolve("game_fallback_ignore_list.js", nullptr);
		size_t i;
		json_t *value;
		json_array_foreach(list, i, value) {
			game_fallback_ignore_list.insert(json_string_value(value));
		}
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
static void run_with_game_fallback(const char *game_id, const char *build, const char *fn, T func)
{
	if (game_fallback_ignore_list.find(fn) != game_fallback_ignore_list.end()) {
		return;
	}
	json_t *j_game_id = nullptr;
	json_t *j_build = nullptr;
	if (game_id) {
		j_game_id = json_string(game_id);
	}
	if (build) {
		j_build = json_string(build);
	}
	change_game_id(&j_game_id, &j_build);
	func();
	change_game_id(&j_game_id, &j_build);
}

size_t get_cv2_size_for_th123(const char *fn, json_t*, size_t)
{
	EnterCriticalSection(&cs);
	size_t size = get_image_data_size(fn, true);
	if (size == 0) {
		run_with_game_fallback("th105", nullptr, fn, [fn, &size]() {
			size = get_image_data_size(fn, true);
		});
	}
	LeaveCriticalSection(&cs);
	return 17 + size;
}

int patch_cv2_for_th123(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
	EnterCriticalSection(&cs);
	int ret = patch_cv2(file_inout, size_out, size_in, fn, patch);
	if (ret <= 0) {
		run_with_game_fallback("th105", nullptr, fn, [file_inout, size_out, size_in, fn, patch, &ret]() {
			ret = patch_cv2(file_inout, size_out, size_in, fn, patch);
		});
	}
	LeaveCriticalSection(&cs);
	return ret;
}

int patch_dat_for_png_for_th123(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
	EnterCriticalSection(&cs);
	int ret = 0;
	run_with_game_fallback("th105", nullptr, fn, [file_inout, size_out, size_in, fn, patch, &ret]() {
		ret = patch_dat_for_png(file_inout, size_out, size_in, fn, patch);
	});
	int ret2 = patch_dat_for_png(file_inout, size_out, size_in, fn, patch);
	LeaveCriticalSection(&cs);
	return max(ret, ret2);
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
	EnterCriticalSection(&cs);
	int ret = BP_file_header(regs, new_bp_info);

	if (game_fallback) {
		// If no file was found, try again with the files from another game.
		// Used for th123, that loads the file from th105.
		file_rep_t *fr = file_rep_get(uFilename);
		if (!fr || (!fr->rep_buffer && !fr->patch)) {
			run_with_game_fallback(json_string_value(game_fallback), nullptr, uFilename, [regs, new_bp_info, &ret]() {
				ret = BP_file_header(regs, new_bp_info);
			});
		}
	}

	LeaveCriticalSection(&cs);
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

static void th105_patch(const file_rep_t *fr, BYTE *buffer, size_t size)
{
	nsml_patch(fr, buffer, size);
	const char *ext = PathFindExtensionA(fr->name);
	if (!strcmp(ext, ".cv0") || !strcmp(ext, ".cv1")) {
		unsigned char xorval = 0x8b;
		unsigned char xoradd = 0x71;
		unsigned char xoraddadd = 0x95;
		for (unsigned int i = 0; i < size; i++) {
			buffer[i] ^= xorval;
			xorval += xoradd;
			xoradd += xoraddadd;
		}
	}
}

int BP_nsml_read_file(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	const char *file_name = (const char*)json_object_get_immediate(bp_info, regs, "file_name");
	// ----------

	// bp_info may be used by several threads at the same time, so we can't change its values.
	json_t *new_bp_info = json_deep_copy(bp_info);
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
	else if (game_id == TH105 || game_id == TH123) {
		json_object_set_new(new_bp_info, "post_read", json_integer((json_int_t)th105_patch));
		json_object_set_new(new_bp_info, "post_patch", json_integer((json_int_t)th105_patch));
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
