/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Main include file.
  */

#pragma once

#include <jansson.h>

struct FileHeader
{
	DWORD filename_hash;
	DWORD unknown;
	DWORD offset;
	DWORD size;
};

struct FileHeaderFull
{
	DWORD filename_hash;
	DWORD unknown;
	DWORD offset;
	DWORD size;

	DWORD key[4];
	void* file_rep;
	size_t file_rep_size;
	DWORD effective_offset;
	char path[MAX_PATH];
};

#ifdef __cplusplus
extern "C" {
#endif

int LoadFileNameList(const char* FileName);
int LoadFileNameListFromMemory(char* list, size_t size);
DWORD filename_to_hash(const char* filename);
struct FileHeaderFull* register_file_header(struct FileHeader* header, DWORD *key);
struct FileHeaderFull* hash_to_file_header(DWORD hash);
DWORD crypt_block(BYTE* Data, DWORD FileSize, DWORD* Key);
int patch_pl(BYTE *file_inout, size_t size_out, size_t size_in, json_t *patch);

#ifdef __cplusplus
}
#endif
