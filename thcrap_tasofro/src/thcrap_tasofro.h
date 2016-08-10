/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Main include file.
  */

#pragma once

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
	char path[MAX_PATH];
};

#ifdef __cplusplus
extern "C" {
#endif

int LoadFileNameList(const char* FileName);
int LoadFileNameListFromMemory(char* list, size_t size);
DWORD filename_to_hash(char* filename);
struct FileHeaderFull* register_file_header(struct FileHeader* header, DWORD *key);
struct FileHeaderFull* hash_to_file_header(DWORD hash);
DWORD crypt_block(BYTE* Data, DWORD FileSize, DWORD* Key);

#ifdef __cplusplus
}
#endif
