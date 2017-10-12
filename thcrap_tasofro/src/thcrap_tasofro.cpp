/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Plugin entry point and breakpoints.
  */

#include <thcrap.h>
#include <vfs.h>
#include "thcrap_tasofro.h"
#include "pl.h"
#include "tfcs.h"
#include "act-nut.h"
#include "spellcards_generator.h"
#include "bgm.h"
#include "plugin.h"
#include "crypt.h"

tasofro_game_t game_id;

// Translate strings to IDs.
static tasofro_game_t game_id_from_string(const char *game)
{
	if (game == NULL) {
		return TH_NONE;
	}
	else if (!strcmp(game, "th135")) {
		return TH135;
	}
	else if (!strcmp(game, "th145")) {
		return TH145;
	}
	return TH_FUTURE;
}

// TODO: read the file names list in JSON format
int __stdcall thcrap_plugin_init()
{
	char* filenames_list;
	size_t filenames_list_size;

	int base_tasofro_removed = stack_remove_if_unneeded("base_tasofro");
	if (base_tasofro_removed == 1) {
		return 1;
	}

	const char *game = json_object_get_string(runconfig_get(), "game");
	game_id = game_id_from_string(game);

	if(base_tasofro_removed == -1) {
		if(game_id == TH145) {
			log_mboxf(NULL, MB_OK | MB_ICONINFORMATION,
				"Support for TH14.5 has been moved out of the sandbox.\n"
				"\n"
				"Please reconfigure your patch stack; otherwise, you might "
				"encounter errors or missing functionality after further "
				"updates.\n"
			);
		} else {
			return 1;
		}
	}

	const char *crypt = json_object_get_string(runconfig_get(), "crypt");
	if (game_id >= TH145) {
		ICrypt::instance = new CryptTh145();
	}
	else {
		ICrypt::instance = new CryptTh135();
	}
	
	patchhook_register("*/stage*.pl", patch_pl);
	patchhook_register("*/ed_*.pl", patch_pl);
	patchhook_register("*.csv", patch_tfcs);
	patchhook_register("*.dll", patch_dll);
	patchhook_register("*.act", patch_act);
	patchhook_register("*.nut", patch_nut);
	patchhook_register("*.txt", patch_plaintext);

	jsonvfs_game_add("data/csv/story/*/stage*.csv.jdiff",						{ "spells.js" }, spell_story_generator);
	if (game_id >= TH145) {
		jsonvfs_game_add("data/csv/spellcard/*.csv.jdiff",						{ "spells.js" }, spell_player_generator);
		jsonvfs_game_add("data/system/char_select3/*/equip/*/000.png.csv.jdiff",{ "spells.js" }, spell_char_select_generator);
	}
	else {
		jsonvfs_game_add("data/csv/Item*.csv.jdiff",							{ "spells.js" }, spell_player_generator);
	}

	char *bgm_pattern_fn = fn_for_game("data/bgm/bgm.csv.jdiff");
	char *musiccmt_fn = fn_for_game("musiccmt.js");
	jsonvfs_add(bgm_pattern_fn, { "themes.js", musiccmt_fn }, bgm_generator);
	SAFE_FREE(musiccmt_fn);
	SAFE_FREE(bgm_pattern_fn);

	filenames_list = (char*)stack_game_file_resolve("fileslist.txt", &filenames_list_size);
	LoadFileNameListFromMemory(filenames_list, filenames_list_size);
	return 0;
}


int BP_file_header(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	struct FileHeader *header = (struct FileHeader*)json_object_get_immediate(bp_info, regs, "struct");
	DWORD *key = (DWORD*)json_object_get_immediate(bp_info, regs, "key");
	// ----------

	if (!header || !key)
		return 1;

	struct FileHeaderFull *full_header = register_file_header(header, key);
	file_rep_t *fr = &full_header->fr;

	if (full_header->path[0]) {
		file_rep_init(fr, full_header->path);
	}

	// If the game loads a DDS file and we have no corresponding DDS file, try to replace it with a PNG file (the game will deal with it)
	if (fr->rep_buffer == NULL && strlen(fr->name) > 4 && strcmp(fr->name + strlen(fr->name) - 4, ".dds") == 0) {
		char png_path[MAX_PATH];
		strcpy(png_path, full_header->path);
		strcpy(png_path + strlen(full_header->path) - 3, "png");
		file_rep_clear(fr);
		file_rep_init(fr, png_path);
	}

	if (fr->rep_buffer != NULL) {
		full_header->size = fr->pre_json_size + fr->patch_size;
		header->size = full_header->size;

		fr->game_buffer = malloc(full_header->size);
		memcpy(fr->game_buffer, fr->rep_buffer, fr->pre_json_size);
		patchhooks_run( // TODO: replace with file_rep_hooks_run
			fr->hooks, fr->game_buffer, fr->pre_json_size + fr->patch_size, fr->pre_json_size, fr->patch
			);

		ICrypt::instance->cryptBlock((BYTE*)fr->game_buffer, full_header->size, full_header->key);
	}

	if (fr->rep_buffer == NULL && fr->patch != NULL) {
		// If we have no rep buffer but we have a patch buffer, we will patch the file later, so we need to keep the file_rep.
		header->size += fr->patch_size;
		full_header->size = header->size;
	}
	else {
		file_rep_clear(fr);
	}

	return 1;
}

/**
  * Replace file, 3rd attempt - hopefully I won't have to write a 4th one.
  * This breakpoint takes care of patching the files.
  * It takes place in the game's file reader, right after it calls its ReadFile caller.
  * At this point, we have the game's file structure in ESI, and so the filename's hash - we can't
  * read the wrong file.
  * In th145, the game's file structure have the following fields:
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
  * We use hFile (file_struct + 4), buffer (file_struct + 8) and FileNameHash (file_struct + 0x1001c).
  * All the other fields are listed for documentation.
  */
int BP_replace_file(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	BYTE *newBuffer = (BYTE*)json_object_get_immediate(bp_info, regs, "buffer");
	DWORD newSize = json_object_get_immediate(bp_info, regs, "size");
	DWORD **ppNumberOfBytesRead = (DWORD**)json_object_get_register(bp_info, regs, "pNumberOfBytesRead");
	HANDLE hFile = (HANDLE)json_object_get_immediate(bp_info, regs, "hFile");
	DWORD hash = json_object_get_immediate(bp_info, regs, "hash");
	// ----------

	static DWORD size = 0;
	static BYTE *buffer = NULL;
	static DWORD *pNumberOfBytesRead = NULL;

	if (newBuffer) {
		buffer = newBuffer;
	}
	if (newSize) {
		size = newSize;
	}
	if (ppNumberOfBytesRead) {
		pNumberOfBytesRead = *ppNumberOfBytesRead;
	}

	if (!hFile || !hash) {
		return 1;
	}

	struct FileHeaderFull *header = hash_to_file_header(hash);
	if (!header || !header->path[0] || (!header->fr.game_buffer && !header->fr.patch)) {
		// Nothing to patch.
		return 1;
	}

	DWORD numberOfBytesRead;
	if (size == 0) {
		size = 65536;
	}
	if (pNumberOfBytesRead) {
		numberOfBytesRead = *pNumberOfBytesRead;
	}
	else {
		numberOfBytesRead = size;
	}

	if (header->effective_offset == -1) {
		header->effective_offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT) - numberOfBytesRead;

		// We couldn't patch the file earlier, but now we have a hFile and an offset, so we should be able to.
		if (header->fr.game_buffer == NULL && header->fr.patch != NULL) {
			SetFilePointer(hFile, -(int)numberOfBytesRead, NULL, FILE_CURRENT);

			DWORD nbOfBytesRead;
			header->fr.game_buffer = malloc(header->size);
			ReadFile(hFile, header->fr.game_buffer, header->orig_size, &nbOfBytesRead, NULL);

			ICrypt::instance->uncryptBlock((BYTE*)header->fr.game_buffer, header->orig_size, header->key);
			patchhooks_run(
				header->fr.hooks, header->fr.game_buffer, header->size, header->orig_size, header->fr.patch
				);
			ICrypt::instance->cryptBlock((BYTE*)header->fr.game_buffer, header->size, header->key);

			SetFilePointer(hFile, header->effective_offset + numberOfBytesRead, NULL, FILE_BEGIN);
			file_rep_clear(&header->fr);
		}
	}

	DWORD offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT) - numberOfBytesRead - header->effective_offset;
	DWORD copy_size;
	if (offset <= header->size) {
		copy_size = min(header->size - offset, size);
	}
	else {
		copy_size = 0;
	}

	log_printf("[replace_file]  known path: %s, hash %.8x, offset: %u, requested size %u, file_rep_size left: %d, chosen size: %u\n",
		header->path, hash, offset, size, header->size - offset, copy_size);
	memcpy(buffer, (BYTE*)header->fr.game_buffer + offset, copy_size);
	if (pNumberOfBytesRead && *pNumberOfBytesRead != copy_size) {
		SetFilePointer(hFile, copy_size - *pNumberOfBytesRead, NULL, FILE_CURRENT);
		*pNumberOfBytesRead = copy_size;
	}

	pNumberOfBytesRead = nullptr;
	buffer = nullptr;
	size = 0;
	return 1;
}
