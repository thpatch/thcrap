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

int LoadFileNameList(const char* FileName);
int LoadFileNameListFromMemory(char* list, size_t size);
DWORD filename_to_hash(const char* filename);
struct FileHeaderFull* register_file_header(struct FileHeader* header, DWORD *key);
struct FileHeaderFull* hash_to_file_header(DWORD hash);
void uncrypt_block(BYTE* Data, DWORD FileSize, DWORD* Key);
DWORD crypt_block(BYTE* Data, DWORD FileSize, DWORD* Key);
int patch_pl(BYTE *file_inout, size_t size_out, size_t size_in, json_t *patch);

#ifdef __cplusplus
}
#endif
