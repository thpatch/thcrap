/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Files list handling.
  */

#include <thcrap.h>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <unordered_map>
#include "thcrap_tasofro.h"
#include "files_list.h"
#include "crypt.h"

std::unordered_map<DWORD, FileHeader> fileHashToName;

// Used if dat_dump != false.
// Contains fileslist.js plus all the files found while the game was running.

class FileslistDump
{
private:
	static FileslistDump *instance;

	std::filesystem::path fileslist_path;
	std::set<std::string> files_list;
	std::mutex mutex;
	std::queue<std::string> queue;
	std::condition_variable cv;

	FileslistDump()
	{
		const char *dat_dump = runconfig_dat_dump_get();

		if (dat_dump) {
			std::filesystem::path dir = std::filesystem::absolute(dat_dump);
			std::filesystem::create_directories(dir);
			this->fileslist_path = dir / "fileslist.js";

			new std::thread([this]() { this->threadProc(); });
		}
	}

	void threadProc()
	{
		Sleep(1000);

		std::unique_lock lock(this->mutex, std::defer_lock);
		while (true) {
			lock.lock();
			while (queue.empty()) {
				this->cv.wait(lock);
			}
			std::string fn = this->queue.front();
			this->queue.pop();
			lock.unlock();

			this->files_list.insert(fn);
			// Filenames tend to arrive in the queue in bursts.
			// We want to convert the filenames list to json and dump
			// it to the disk only then the burst is finished.
			if (this->queue.empty()) {
				Sleep(200);
			}
			if (!this->queue.empty()) {
				continue;
			}

			// Save files list
			ScopedJson files_list_json = json_array();
			for (auto& it : this->files_list) {
				char *path_u = EnsureUTF8(it.c_str(), it.length());
				str_slash_normalize_win(path_u);
				json_array_append_new(*files_list_json, json_string(path_u));
				free(path_u);
			}
			json_dump_file(*files_list_json, this->fileslist_path.generic_u8string().c_str(), JSON_INDENT(2));
		}
	}

	void add_(const std::string& fn)
	{
		if (this->fileslist_path.empty()) {
			return;
		}

		std::scoped_lock lock(this->mutex);
		this->queue.push(fn);
		this->cv.notify_one();
	}

public:
	static void add(std::string fn)
	{
		if (instance == nullptr) {
			instance = new FileslistDump();
		}
		instance->add_(fn);
	}
};
FileslistDump *FileslistDump::instance;

void register_filename(const char *path)
{
	if (!path || path[1] == ':') {
		return;
	}

	DWORD hash = ICrypt::instance->SpecialFNVHash(path, path + strlen(path));
	strcpy(fileHashToName[hash].path, path);

	FileslistDump::add(path);
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

void register_utf8_filename(const char* file)
{
	WCHAR_T_DEC(file);
	WCHAR_T_CONV(file);
	VLA(char, file_sjis, file_len);
	WideCharToMultiByte(932, 0, file_w, wcslen(file_w) + 1, file_sjis, file_len, nullptr, nullptr);
	register_filename(file_sjis);
	VLA_FREE(file_sjis);
	WCHAR_T_FREE(file);
}

// Convert fileslist.txt to fileslist.js:
// iconv -f sjis fileslist.txt | sed -e 'y|¥/|\\\\|' | jq -Rs '. | split("\n") | sort' > fileslist.js
int LoadFileNameListFromJson(json_t *fileslist)
{
	size_t i;
	json_t *file;
	json_array_foreach(fileslist, i, file) {
		register_utf8_filename(json_string_value(file));
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
