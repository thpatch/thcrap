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

struct FileHeader
{
	// Copy of FileHeader
	DWORD hash;
	// XOR key
	DWORD key[4];
	// file rep
	file_rep_t *fr;
	// File path
	char path[MAX_PATH];
};

#ifdef __cplusplus
extern "C" {
#endif

int th135_init();
	
int LoadFileNameList(const char* FileName);
int LoadFileNameListFromMemory(char* list, size_t size);
int LoadFileNameListFromJson(json_t *fileslist);
void register_filename(const char *path);
DWORD filename_to_hash(const char* filename);
struct FileHeader* hash_to_file_header(DWORD hash);
json_t *custom_stack_game_json_resolve(const char *fn, size_t *size, json_t *patches);

int patch_plaintext(void *file_inout, size_t size_out, size_t size_in, const char*, json_t *patch);

#ifdef __cplusplus
}
#endif
