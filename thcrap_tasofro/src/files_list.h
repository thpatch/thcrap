/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Files list handling.
  */

#pragma once

#include <thcrap.h>
#include <bp_file.h>

struct FileHeader
{
	DWORD hash;
	// XOR key (th135 only)
	DWORD key[4];
	// file rep
	file_rep_t *fr;
	// File path
	char path[MAX_PATH];
};

#ifdef __cplusplus
extern "C" {
#endif

	int LoadFileNameList(const char* FileName);
	int LoadFileNameListFromMemory(char* list, size_t size);
	int LoadFileNameListFromJson(json_t *fileslist);
	void register_filename(const char *path);
	DWORD filename_to_hash(const char* filename);
	struct FileHeader* hash_to_file_header(DWORD hash);

#ifdef __cplusplus
}
#endif
