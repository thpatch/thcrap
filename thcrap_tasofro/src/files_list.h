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
#include <filesystem>
#include "tasofro_file.h"

#ifdef __cplusplus
struct Th135File : public TasofroFile
{
	// Filename hash
	DWORD hash = 0;
	// XOR key (th135 only)
	DWORD key[4];
	// File path
	std::filesystem::path path;

	static Th135File* tls_get();
	static void tls_set(Th135File *file);
};

extern "C" {
#endif

	int LoadFileNameList(const char* FileName);
	int LoadFileNameListFromMemory(char* list, size_t size);
	int LoadFileNameListFromJson(json_t *fileslist);
	void register_filename(const char *path);
	void register_utf8_filename(const char *path);
	DWORD filename_to_hash(const char* filename);

#ifdef __cplusplus
}

Th135File *hash_to_Th135File(DWORD hash);

#endif
