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
#include "cv0.h"
#include "nsml_images.h"
#include <set>

/// Detour chains
/// -------------
W32U8_DETOUR_CHAIN_DEF(GetGlyphOutline);
/// -------------

static CRITICAL_SECTION cs;
static std::set<const char*, bool(*)(const char*, const char*)> game_fallback_ignore_list([](const char *a, const char *b){ return strcmp(a, b) < 0; });

// Copy-paste of fn_for_game from patchfile.cpp
static char* fn_for_th105(const char *fn)
{
	const char *game_id = "th105";
	size_t game_id_len = strlen(game_id) + 1;
	char *full_fn;

	if (!fn) {
		return NULL;
	}
	full_fn = (char*)malloc(game_id_len + strlen(fn) + 1);

	full_fn[0] = 0; // Because strcat
	if (game_id) {
		strncpy(full_fn, game_id, game_id_len);
		strcat(full_fn, "/");
	}
	strcat(full_fn, fn);
	return full_fn;
}

json_t *th123_resolve_chain_game(const char *fn)
{
	json_t *ret = nullptr;

	// First, th105
	if (game_fallback_ignore_list.find(fn) == game_fallback_ignore_list.end()) {
		char *fn_game = fn_for_th105(fn);
		ret = resolve_chain(fn_game);
		SAFE_FREE(fn_game);
	}

	char *fn_common = fn_for_game(fn);
	const char *fn_common_ptr = fn_common ? fn_common : fn;
	json_t *th123_ret = resolve_chain(fn_common_ptr);
	if (ret && th123_ret) {
		json_array_extend(ret, th123_ret);
		json_decref(th123_ret);
	}
	else if (!ret && th123_ret) {
		ret = th123_ret;
	}
	SAFE_FREE(fn_common);
	return ret;
}

int nsml_init()
{
	if (game_id == TH_MEGAMARI) {
		patchhook_register("*.bmp", patch_bmp, get_bmp_size);
	}
	else if (game_id == TH_NSML) {
		// Increasing the file size makes the game crash.
		patchhook_register("*.cv2", patch_cv2, nullptr);
	}
	else if (game_id == TH105 || game_id == TH123) {
		patchhook_register("*.cv0", patch_cv0, nullptr);
		patchhook_register("*.cv1", patch_csv, nullptr);
		patchhook_register("*.cv2", patch_cv2, get_cv2_size);
		patchhook_register("*.dat", patch_dat_for_png, [](const char*, json_t*, size_t) -> size_t { return 0; });
	}

	if (game_id == TH105) {
		char *bgm_fn = fn_for_game("data/csv/system/music.cv1.jdiff");
		jsonvfs_add(bgm_fn, { "themes.js" }, bgm_generator);
		SAFE_FREE(bgm_fn);

		jsonvfs_game_add_map("data/csv/*/spellcard.cv1.jdiff", "spells.js");
		jsonvfs_game_add_map("data/csv/*/storyspell.cv1.jdiff", "spells.js");
	}
	else if (game_id == TH123) {
		set_resolve_chain_game(th123_resolve_chain_game);
		json_t *list = stack_game_json_resolve("game_fallback_ignore_list.js", nullptr);
		size_t i;
		json_t *value;
		json_array_foreach(list, i, value) {
			game_fallback_ignore_list.insert(json_string_value(value));
		}

		char *bgm_fn = fn_for_game("data/csv/system/music*.cv1.jdiff");
		jsonvfs_add_map(bgm_fn, "themes.js");
		SAFE_FREE(bgm_fn);

		char *pattern_spell = fn_for_game("data/csv/*/spellcard.cv1.jdiff");
		char *pattern_story = fn_for_game("data/csv/*/storyspell.cv1.jdiff");
		char *spells_th105 = fn_for_th105("spells.js");
		char *spells_th123 = fn_for_game("spells.js");
		jsonvfs_add_map(pattern_spell, spells_th105);
		jsonvfs_add_map(pattern_spell, spells_th123);
		jsonvfs_add_map(pattern_story, spells_th105);
		jsonvfs_add_map(pattern_story, spells_th123);
		SAFE_FREE(pattern_spell);
		SAFE_FREE(pattern_story);
		SAFE_FREE(spells_th105);
		SAFE_FREE(spells_th123);
	}
	return 0;
}

/**
  * Convert the file_name member of bp_info to a lowercase UTF-8 string.
  * If bp_info contains a fn_size member, this member must contains the size
  * of file_name (in bytes). Otherwise, file_name must be null-terminated.
  *
  * After the call, uFilename will contain a pointer to the utf-8 file name.
  * The caller have to free() it.
  */
json_t *convert_file_name_in_bp(x86_reg_t *regs, json_t *bp_info, char **uFilename)
{
	// Parameters
	// ----------
	const char *filename = (const char*)json_object_get_immediate(bp_info, regs, "file_name");
	size_t fn_size = json_object_get_immediate(bp_info, regs, "fn_size");
	// ----------

	if (fn_size) {
		*uFilename = EnsureUTF8(filename, fn_size);
	}
	else {
		*uFilename = EnsureUTF8(filename, strlen(filename));
	}
	CharLowerA(*uFilename);

	json_t *new_bp_info = json_copy(bp_info);
	json_object_set_new(new_bp_info, "file_name", json_integer((json_int_t)*uFilename));

	return new_bp_info;
}

int BP_nsml_file_header(x86_reg_t *regs, json_t *bp_info)
{
	char *uFilename;
	json_t *new_bp_info = convert_file_name_in_bp(regs, bp_info, &uFilename);

	EnterCriticalSection(&cs);
	int ret = BP_file_header(regs, new_bp_info);
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
	EnterCriticalSection(&cs);
	// bp_info may be used by several threads at the same time, so we can't change its values.
	char *uFilename = nullptr;
	json_t *new_bp_info = convert_file_name_in_bp(regs, bp_info, &uFilename);
	LeaveCriticalSection(&cs);

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
int BP_th105_open_file(x86_reg_t *regs, json_t *bp_info)
{
	EnterCriticalSection(&cs);
	char *uFilename;
	json_t *new_bp_info = convert_file_name_in_bp(regs, bp_info, &uFilename);
	LeaveCriticalSection(&cs);

	int ret = BP_fragmented_open_file(regs, new_bp_info);

	json_decref(new_bp_info);
	free(uFilename);
	return ret;
}

int BP_th105_font_spacing(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	size_t font_size     = json_object_get_immediate(bp_info, regs, "font_size");
	size_t *y_offset     = json_object_get_pointer(bp_info, regs, "y_offset");
	size_t *font_spacing = json_object_get_pointer(bp_info, regs, "font_spacing");
	// ----------

	if (!font_size) {
		return 1;
	}

	json_t *entry = json_object_numkey_get(json_object_get(bp_info, "new_y_offset"), font_size);
	if (entry && json_is_integer(entry) && y_offset) {
		*y_offset  = (size_t)json_integer_value(entry);
	}

	entry = json_object_numkey_get(json_object_get(bp_info, "new_spacing"), font_size);
	if (entry && json_is_integer(entry) && font_spacing) {
		*font_spacing = (size_t)json_integer_value(entry);
	}
	return 1;
}

DWORD WINAPI th105_GetGlyphOutlineU(HDC hdc, UINT uChar, UINT uFormat, LPGLYPHMETRICS lpgm, DWORD cbBuffer, LPVOID lpvBuffer, const MAT2 *lpmat2)
{
	uChar = CharToUTF16(uChar);

	if (uChar & 0xFFFF0000 ||
		font_has_character(hdc, uChar)) {
		// is_character_in_font won't work if the character doesn't fit into a WCHAR.
		// When it happens, we'll be optimistic and hope our font have that character.
		return GetGlyphOutlineW(hdc, uChar, uFormat, lpgm, cbBuffer, lpvBuffer, lpmat2);
	}

	HFONT origFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
	LOGFONTW lf;
	GetObjectW(origFont, sizeof(lf), &lf);
	HFONT newFont = font_create_for_character(&lf, uChar);
	if (newFont) {
		origFont = (HFONT)SelectObject(hdc, newFont);
	}
	int ret = GetGlyphOutlineW(hdc, uChar, uFormat, lpgm, cbBuffer, lpvBuffer, lpmat2);
	if (newFont) {
		SelectObject(hdc, origFont);
		DeleteObject(newFont);
	}
	return ret;
}

extern "C" int nsml_mod_init()
{
	InitializeCriticalSection(&cs);
	return 0;
}

extern "C" void nsml_mod_detour(void)
{
	detour_chain("gdi32.dll", 0,
		"GetGlyphOutlineA", th105_GetGlyphOutlineU,
		NULL
		);
}

extern "C" void nsml_mod_exit()
{
	DeleteCriticalSection(&cs);
}
