/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Plugin breakpoints and hooks for th135+.
  */

#include <thcrap.h>
#include <vfs.h>
#include "thcrap_tasofro.h"
#include "files_list.h"
#include "pl.h"
#include "tfcs.h"
#include "act-nut.h"
#include "spellcards_generator.h"
#include "bgm.h"
#include "plugin.h"
#include "nhtex.h"
#include "th155_bmp_font.h"
#include "crypt.h"

int th135_init()
{
	if (game_id >= TH145) {
		ICrypt::instance = new CryptTh145();
	}
	else {
		ICrypt::instance = new CryptTh135();
	}
	
	patchhook_register("*/stage*.pl", patch_pl, nullptr);
	patchhook_register("*/ed*.pl", patch_pl, nullptr);
	patchhook_register("*.csv", patch_tfcs, get_tfcs_size);
	patchhook_register("*.dll", patch_dll, [](const char*, json_t*, size_t) -> size_t { return 0; });
	patchhook_register("*.act", patch_act, nullptr);
	patchhook_register("*.nut", patch_nut, nullptr);
	patchhook_register("*.txt", patch_plaintext, nullptr);
	patchhook_register("*.nhtex", patch_nhtex, get_nhtex_size);

	if (game_id >= TH155) {
		jsonvfs_game_add_map("data/spell/*.csv.jdiff",							"spells.js");
		jsonvfs_game_add_map("data/story/spell_list/*.csv.jdiff",				"spells.js");
		jsonvfs_game_add_map("data/actor/*.nut.jdiff",							"spells.js"); // Last words
		jsonvfs_game_add_map("*.nut.jdiff",										"nut_strings.js");
		patchhook_register("data/font/*.bmp", patch_bmp_font, get_bmp_font_size);
	}
	else if (game_id >= TH145) {
		jsonvfs_game_add("data/csv/story/*/stage*.csv.jdiff",					{ "spells.js" }, spell_story_generator);
		jsonvfs_game_add("data/csv/spellcard/*.csv.jdiff",						{ "spells.js" }, spell_player_generator);
		jsonvfs_game_add("data/system/char_select3/*/equip/*/000.png.csv.jdiff",{ "spells.js" }, spell_char_select_generator);
	}
	else {
		jsonvfs_game_add("data/csv/story/*/stage*.csv.jdiff",					{ "spells.js" }, spell_story_generator);
		jsonvfs_game_add_map("data/csv/Item*.csv.jdiff",						"spellcomments.js");
		jsonvfs_game_add_map("data/csv/Item*.csv.jdiff",						"spells.js");
	}

	char *bgm_pattern_fn = fn_for_game("data/bgm/bgm.csv.jdiff");
	char *musiccmt_fn = fn_for_game("musiccmt.js");
	jsonvfs_add(bgm_pattern_fn, { "themes.js", musiccmt_fn }, bgm_generator);
	SAFE_FREE(musiccmt_fn);
	SAFE_FREE(bgm_pattern_fn);
	if (game_id >= TH155) {
		char *staffroll_fn = fn_for_game("data/system/ed/staffroll.csv.jdiff");
		jsonvfs_add_map(staffroll_fn, "themes.js");
		SAFE_FREE(staffroll_fn);
	}

	json_t *fileslist = stack_game_json_resolve("fileslist.js", nullptr);
	LoadFileNameListFromJson(fileslist);
	json_decref(fileslist);
	return 0;
}


bool th135_init_fr(Th135File *fr, std::filesystem::path path)
{
	fr->init(path.generic_u8string().c_str());
	if (fr->need_replace()) {
		return true;
	}

	file_rep_clear(fr);

	// If the game loads a DDS file and we have no corresponding DDS file,
	// try to replace it with a PNG file (the game will deal with it)
	if (path.extension() == ".dds") {
		path.replace_extension(".png");
		register_utf8_filename(path.generic_u8string().c_str());

		return th135_init_fr(fr, path);
	}

	return false;
}

extern "C" int BP_th135_file_header(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	DWORD hash = json_object_get_immediate(bp_info, regs, "file_hash");
	size_t *size = json_object_get_pointer(bp_info, regs, "file_size");
	DWORD *key = (DWORD*)json_object_get_immediate(bp_info, regs, "file_key");
	// ----------

	if (!hash || !key)
		return 1;

	Th135File *fr = hash_to_Th135File(hash);
	if (!fr) {
		return 1;
	}
	if (fr->hash == 0) {
		if (!th135_init_fr(fr, fr->path)) {
			return 1;
		}
	}

	fr->hash = hash;
	if (!fr->rep_buffer) {
		// We will need to allocate a buffer big enough for the original game file
		fr->pre_json_size = *size;
	}
	memcpy(fr->key, key, 4 * sizeof(DWORD));
	ICrypt::instance->convertKey(fr->key);

	*size = MAX(*size, fr->pre_json_size) + fr->patch_size;

	return 1;
}

extern "C" int BP_th135_file_name(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	const char *filename = (const char*)json_object_get_immediate(bp_info, regs, "file_name");
	// ----------

	if (filename) {
		register_filename(filename);
	}
	return 1;
}

/**
  * In th145, the game's file structure have the following fields:
  *
  * void* vtable;
  * HANDLE hFile;
  * BYTE buffer[0x10000];
  * DWORD LastReadFileSize; // Last value returned in lpNumberOfBytesRead in ReadFile
  * DWORD Offset; // Offset for the reader. I don't know what happens to it when it's bigger than 0x10000
  * DWORD LastReadSize; // Last read size asked to the reader
  * DWORD unknown1;
  * DWORD FileSize;
  * DWORD FileNameHash;
  * DWORD unknown2;
  * DWORD XorOffset; // Offset used by the XOR function
  * DWORD Key[5]; // 32-bytes encryption key used for XORing. The 5th DWORD is a copy of the 1st DWORD.
  * DWORD Aux; // Contains the last DWORD read from the file. Used during XORing.
  */

extern "C" int BP_th135_prepareReadFile(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	DWORD hash = json_object_get_immediate(bp_info, regs, "hash");
	// ----------

	Th135File::tls_set(nullptr);

	if (!hash) {
		return 1;
	}

	Th135File *fr = hash_to_Th135File(hash);
	if (!fr || !fr->need_replace()) {
		// Nothing to patch.
		return 1;
	}

	log_printf("Patching %s...\n", fr->path.filename().u8string().c_str());
	EnterCriticalSection(&fr->cs);
	Th135File::tls_set(fr);
	return 1;
}

extern "C" int BP_th135_replaceReadFile(x86_reg_t *regs, json_t*)
{
	Th135File *fr = Th135File::tls_get();
	if (!fr) {
		return 1;
	}

	int ret = fr->replace_ReadFile(regs,
		[fr](TasofroFile*, BYTE *buffer, DWORD size) { ICrypt::instance->uncryptBlock(buffer, size, fr->key); },
		[fr](TasofroFile*, BYTE *buffer, DWORD size) { ICrypt::instance->cryptBlock(  buffer, size, fr->key); }
	);

	LeaveCriticalSection(&fr->cs);
	Th135File::tls_set(nullptr);
	return ret;
}
