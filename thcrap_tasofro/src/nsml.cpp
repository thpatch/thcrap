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
#include "tasofro_file.h"
#include "tfcs.h"
#include "bgm.h"
#include "cv0.h"
#include "nsml_images.h"
#include <map>
#include <set>

/// Detour chains
/// -------------
W32U8_DETOUR_CHAIN_DEF(GetGlyphOutline);
/// -------------

static CRITICAL_SECTION cs;
std::map<std::string, TasofroFile> files_list;
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

size_t get_chain_size(char **chain)
{
	size_t i;
	for (i = 0; chain && chain[i]; i++) {
	}
	return i;
}

char **th123_resolve_chain_game(const char *fn)
{
	// First, th105
	char **th105_chain = nullptr;
	defer(free(th105_chain));
	if (game_fallback_ignore_list.find(fn) == game_fallback_ignore_list.end()) {
		char *fn_game = fn_for_th105(fn);
		th105_chain = resolve_chain(fn_game);
		SAFE_FREE(fn_game);
	}

	// Then, th123
	char *fn_common = fn_for_game(fn);
	const char *fn_common_ptr = fn_common ? fn_common : fn;
	char **th123_chain = resolve_chain(fn_common_ptr);
	defer(free(th123_chain));
	SAFE_FREE(fn_common);

	// Merge the 2 chains
	size_t chain_size = get_chain_size(th105_chain) + get_chain_size(th123_chain);
	if (chain_size == 0) {
		return nullptr;
	}
	char **chain = (char**)malloc(sizeof(char*) * (chain_size + 1));
	size_t i = 0;
	for (size_t j = 0; th105_chain && th105_chain[j]; i++, j++) {
		chain[i] = th105_chain[j];
	}
	for (size_t j = 0; th123_chain && th123_chain[j]; i++, j++) {
		chain[i] = th123_chain[j];
	}
	chain[i] = nullptr;

	return chain;
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
		patchhook_register("*.cv1", patch_csv, get_csv_size);
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

static void megamari_xor(const TasofroFile *fr, BYTE *buffer, size_t size)
{
	BYTE key = ((fr->offset >> 1) | 8) & 0xFF;
	for (unsigned int i = 0; i < size; i++) {
		buffer[i] ^= key;
	}
}

static void nsml_xor(const TasofroFile *fr, BYTE *buffer, size_t size)
{
	BYTE key = ((fr->offset >> 1) | 0x23) & 0xFF;
	for (unsigned int i = 0; i < size; i++) {
		buffer[i] ^= key;
	}
}

static void th105_xor(const TasofroFile *fr, BYTE *buffer, size_t size)
{
	nsml_xor(fr, buffer, size);

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

static void game_xor(TasofroFile *fr, BYTE *buffer, size_t size)
{
	if (game_id == TH_MEGAMARI) {
		return megamari_xor(fr, buffer, size);
	}
	else if (game_id == TH105 || game_id == TH123) {
		return th105_xor(fr, buffer, size);
	}
	else {
		return nsml_xor(fr, buffer, size);
	}
}


// Class name from the game's RTTI. Inherits from CFileHeader
struct CPackageFileReader
{
	void *vtable;
	HANDLE hFile;
	size_t numberOfBytesRead; // Return of the last ReadFile call
	size_t file_size;
	size_t offset_in_archive;
	size_t current_offset;
	BYTE xor_key;
};

extern "C" int BP_nsml_CPackageFileReader_openFile(x86_reg_t *regs, json_t * bp_info)
{
	// Parameters
	// ----------
	auto file_name = (char*)json_object_get_immediate(bp_info, regs, "file_name");
	auto file_reader = (CPackageFileReader*)json_object_get_immediate(bp_info, regs, "file_reader");
	// ----------

	if (!file_name || !file_reader) {
		return 1;
	}

	file_name = EnsureUTF8(file_name, strlen(file_name));
	CharLowerA(file_name);

	TasofroFile *fr = new TasofroFile();
	fr->init(file_name);
	free(file_name);

	if (fr->need_replace()) {
		file_reader->file_size = fr->init_game_file_size(file_reader->file_size);
	}
	else {
		file_rep_clear(fr);
		delete fr;
		return 1;
	}

	TasofroFile::tls_set(fr);
	return 1;
}

extern "C" int BP_nsml_CPackageFileReader_readFile(x86_reg_t *regs, json_t*)
{
	TasofroFile *fr = TasofroFile::tls_get();
	if (!fr) {
		return 1;
	}

	return fr->replace_ReadFile(regs, game_xor, game_xor);
}

extern "C" int BP_nsml_CFileReader_closeFile(x86_reg_t *regs, json_t*)
{
	TasofroFile *fr = TasofroFile::tls_get();

	if (fr) {
		LeaveCriticalSection(&cs);
		TasofroFile::tls_set(nullptr);
	}

	return 1;
}

struct MegamariFile
{
	HANDLE hFile;
	DWORD hash;
	DWORD size;
	DWORD unk1; // Changes with the size
	DWORD unk2; // Always uninitialized when creating the file. Not an offset.
	DWORD unk3; // Doesn't change, point to valid memory
	DWORD unk4; // Always 0x465
};

extern "C" int BP_megamari_openFile(x86_reg_t * regs, json_t * bp_info)
{
	// Parameters
	// ----------
	auto file_name = (char*)json_object_get_immediate(bp_info, regs, "file_name");
	auto file_struct = (MegamariFile*)json_object_get_immediate(bp_info, regs, "file_struct");
	// ----------

	if (!file_name || !file_struct) {
		return 1;
	}

	// We didn't look for a closeFile breakpoint, so just close the lase file whenever we open a new one.
	// It seems to work well enough.
	if (TasofroFile::tls_get()) {
		delete TasofroFile::tls_get();
		TasofroFile::tls_set(nullptr);
	}

	file_name = EnsureUTF8(file_name, strlen(file_name));
	CharLowerA(file_name);

	TasofroFile *fr = new TasofroFile();
	fr->init(file_name);
	free(file_name);

	if (fr->need_replace()) {
		file_struct->size = fr->init_game_file_size(file_struct->size);
	}
	else {
		file_rep_clear(fr);
		delete fr;
		return 1;
	}

	TasofroFile::tls_set(fr);
	return 1;
}

// In th105, relying on the last open file doesn't work. So we'll use the file object instead.
std::map<void*, TasofroFile> th105_open_files_list;
extern "C" int BP_th105_open_file(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	char *file_name = (char*)json_object_get_immediate(bp_info, regs, "file_name");
	void *file_object = (void*)json_object_get_immediate(bp_info, regs, "file_object");
	size_t *file_size = json_object_get_pointer(bp_info, regs, "file_size");
	// ----------

	EnterCriticalSection(&cs);

	if (file_name && file_object) {
		TasofroFile& fr = th105_open_files_list[file_object];
		TasofroFile::tls_set(&fr);

		file_name = EnsureUTF8(file_name, strlen(file_name));
		CharLowerA(file_name);
		fr.init(file_name);
		free(file_name);
	}
	else if (file_size) {
		TasofroFile *fr = TasofroFile::tls_get();
		if (fr) {
			*file_size = fr->init_game_file_size(*file_size);
		}
		TasofroFile::tls_set(nullptr);
	}

	LeaveCriticalSection(&cs);
	return 1;
}

extern "C" int BP_th105_replaceReadFile(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	void *file_object = (void*)json_object_get_immediate(bp_info, regs, "file_object");

	if (!file_object) {
		return 1;
	}

	EnterCriticalSection(&cs);
	auto it = th105_open_files_list.find(file_object);
	if (it == th105_open_files_list.end()) {
		LeaveCriticalSection(&cs);
		return 1;
	}
	TasofroFile& fr = it->second;

	ReadFileStack *stack = (ReadFileStack*)(regs->esp + sizeof(void*));
	int ret = fr.replace_ReadFile(regs, game_xor, game_xor);

	LeaveCriticalSection(&cs);
	return ret;
}

extern "C" int BP_th105_close_file(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	void *file_object = (void*)json_object_get_immediate(bp_info, regs, "file_object");
	// ----------

	if (!file_object) {
		return 1;
	}

	EnterCriticalSection(&cs);
	auto it = th105_open_files_list.find(file_object);
	if (it == th105_open_files_list.end()) {
		LeaveCriticalSection(&cs);
		return 1;
	}
	file_rep_t& fr = it->second;

	file_rep_clear(&fr);
	th105_open_files_list.erase(it);
	LeaveCriticalSection(&cs);
	return 1;
}

extern "C" int BP_th105_font_spacing(x86_reg_t *regs, json_t *bp_info)
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
