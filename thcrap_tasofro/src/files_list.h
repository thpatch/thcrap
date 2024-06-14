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
	// XOR key
	DWORD key[4] = {};
};

extern "C" {
#endif

	int LoadFileNameList(const char* FileName);
	int LoadFileNameListFromMemory(char* list, size_t size);
	int LoadFileNameListFromJson(json_t *fileslist);
	void register_filename(const char *path);
	void register_utf8_filename(const char *path);

#ifdef __cplusplus
}

#endif
