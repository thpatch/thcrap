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

std::unordered_map<DWORD, FileHeaderFull> fileHashToName;
int LoadFileNameList(const char* FileName)
{
	FILE* fp = fopen(FileName, "rt");
	if (!fp) return -1;
	char FilePath[MAX_PATH] = { 0 };
	while (fgets(FilePath, MAX_PATH, fp))
	{
		int tlen = strlen(FilePath);
		while (tlen && FilePath[tlen - 1] == '\n') FilePath[--tlen] = 0;
		DWORD thash = ICrypt::instance->SpecialFNVHash(FilePath, FilePath + tlen);
		strcpy(fileHashToName[thash].path, FilePath);
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
		DWORD thash = ICrypt::instance->SpecialFNVHash(list, list + len);
		strncpy(fileHashToName[thash].path, list, len);
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

struct FileHeaderFull* register_file_header(FileHeader* header, DWORD *key)
{
	FileHeaderFull& full_header = fileHashToName[header->filename_hash];

	full_header.filename_hash = header->filename_hash;
	full_header.unknown = header->unknown;
	full_header.offset = header->offset;
	full_header.size = header->size;

	full_header.key[0] = key[0];
	full_header.key[1] = key[1];
	full_header.key[2] = key[2];
	full_header.key[3] = key[3];
	ICrypt::instance->convertKey(full_header.key);
	full_header.effective_offset = -1;
	full_header.orig_size = header->size;

	ZeroMemory(&full_header.fr, sizeof(file_rep_t));

	return &full_header;
}

struct FileHeaderFull* hash_to_file_header(DWORD hash)
{
	std::unordered_map<DWORD, FileHeaderFull>::iterator it = fileHashToName.find(hash);

	if (it != fileHashToName.end())
		return &it->second;
	else
		return NULL;
}