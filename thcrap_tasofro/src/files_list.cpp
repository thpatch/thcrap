/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Files list handling.
  */

#include <thcrap.h>
#include <unordered_map>
#include "thcrap_tasofro.h"
#include "crypt.h"

std::unordered_map<DWORD, FileHeader> fileHashToName;

void register_filename(const char *path)
{
	DWORD hash = ICrypt::instance->SpecialFNVHash(path, path + strlen(path));
	strcpy(fileHashToName[hash].path, path);
}

int LoadFileNameList(const char* FileName)
{
	FILE* fp = fopen(FileName, "rt");
	if (!fp) return -1;
	char FilePath[MAX_PATH] = { 0 };
	while (fgets(FilePath, MAX_PATH, fp))
	{
		int tlen = strlen(FilePath);
		while (tlen && FilePath[tlen - 1] == '\n') FilePath[--tlen] = 0;
		register_filename(FilePath);
	}
	fclose(fp);
	return 0;
}

int LoadFileNameListFromMemory(char* list, size_t size)
{
	while (size > 0)
	{
		size_t len = 0;
		while (len < size && list[len] != '\r' && list[len] != '\n') {
			len++;
		}
		char save = list[len];
		list[len] = 0;
		register_filename(list);
		list[len] = save;
		list += len;
		size -= len;
		while (size > 0 && (list[0] == '\r' || list[0] == '\n')) {
			list++;
			size--;
		}
	}
	return 0;
}

DWORD filename_to_hash(const char* filename)
{
	return ICrypt::instance->SpecialFNVHash(filename, filename + strlen(filename));
}

struct FileHeader* hash_to_file_header(DWORD hash)
{
	std::unordered_map<DWORD, FileHeader>::iterator it = fileHashToName.find(hash);

	if (it != fileHashToName.end())
		return &it->second;
	else
		return NULL;
}