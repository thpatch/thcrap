/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Main include file.
  */

#pragma once

#include <thcrap.h>
#include <bp_file.h>
#include <jansson.h>

typedef enum {
	// • filename hash: uses hash = ch ^ 0x1000193 * hash
	// • spells: using data/csv/Item*.csv
	// • spells: data/csv/story/*/*.csv has columns for popularity
	TH135,

	// • filename hash: uses hash = (hash ^ ch) * 0x1000193
	// • XOR: uses an additional AUX parameter
	// • XOR: key compunents are multiplied by -1
	// • endings: the last line in the text may be on another line in the pl file, and it doesn't wait for an input.
	// • spells: using data/csv/spellcards/*.csv and data/system/char_select3/*/equip/*/000.png.csv
	TH145,

	// Any future game without relevant changes
	TH_FUTURE,
} tasofro_game_t;

extern tasofro_game_t game_id;

struct FileHeader
{
	DWORD filename_hash;
	DWORD unknown;
	DWORD offset;
	DWORD size;
};

struct FileHeaderFull
{
	// Copy of FileHeader
	DWORD filename_hash;
	DWORD unknown;
	DWORD offset;
	DWORD size;

	// XOR key
	DWORD key[4];
	file_rep_t fr;
	// Offset to the file in its pak file
	DWORD effective_offset;
	// Size of the original game file
	DWORD orig_size;

	char path[MAX_PATH];
};

#ifdef __cplusplus
extern "C" {
#endif

int __stdcall thcrap_plugin_init();

int BP_file_header(x86_reg_t *regs, json_t *bp_info);
int BP_replace_file(x86_reg_t *regs, json_t *bp_info);

int LoadFileNameList(const char* FileName);
int LoadFileNameListFromMemory(char* list, size_t size);
DWORD filename_to_hash(const char* filename);
struct FileHeaderFull* register_file_header(struct FileHeader* header, DWORD *key);
struct FileHeaderFull* hash_to_file_header(DWORD hash);

int patch_plaintext(void *file_inout, size_t size_out, size_t size_in, json_t *patch);

#ifdef __cplusplus
}
#endif
