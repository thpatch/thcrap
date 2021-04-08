/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Plugin breakpoints and hooks for th175.
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"
#include "files_list.h"
#include "act-nut.h"

int th175_init()
{
	patchhook_register("*.nut", patch_nut, nullptr);

	return 0;
}

struct AVfile_handle
{
	void *vtable;
	HANDLE hFile;
	uint32_t unk1;
	uint32_t unk2;
};

struct AVFileIO_DefaultReader
{
	void *vtable;
	AVfile_handle handle;
};

struct AVPackageReader
{
	void *vtable;
	DWORD refcount; // from IUnknown
	uint32_t unk_08;
	uint32_t unk_0C;
	uint32_t unk_10;
	uint32_t unk_14;
	uint64_t file_size;
	uint64_t file_offset;
	uint32_t unk_28;
	AVFileIO_DefaultReader *file_reader;
};

static std::map<AVPackageReader*, TasofroFile> open_files;
std::mutex open_files_mutex;

extern "C" int BP_th175_open_file(x86_reg_t *regs, json_t *bp_info)
{
	const char *file_name = (const char*)json_object_get_immediate(bp_info, regs, "file_name");
	AVPackageReader *reader = (AVPackageReader*)json_object_get_immediate(bp_info, regs, "file_reader");

	if (!file_name || !reader) {
		return 1;
	}

	std::scoped_lock<std::mutex> lock(open_files_mutex);
	TasofroFile& fr = open_files[reader];

	fr.init(file_name);
	if (!fr.need_replace()) {
		open_files.erase(reader);
		return 1;
	}

	reader->file_size = fr.init_game_file_size(static_cast<size_t>(reader->file_size));

	return 1;
}

// Thanks to the power of XOR, we can use it for both decryption and encryption
void th175_crypt_file(uint8_t *buffer, size_t size)
{
	uint8_t key[] = "z.e-ahwqb1neai un0dsk;afjv0cx0@z";
	uint8_t cl = 0;

	for (size_t offset = 0; offset < size; offset++) {
		cl = (offset & 0xFF) ^ key[offset & 0x1F];
		buffer[offset] ^= cl;
		buffer[offset] ^= (size & 0xFF);
	}
}

extern "C" int BP_th175_replaceReadFile(x86_reg_t *regs, json_t *bp_info)
{
	AVfile_handle *handle = (AVfile_handle*)json_object_get_immediate(bp_info, regs, "file_handle");

	std::scoped_lock<std::mutex> lock(open_files_mutex);
	auto it = std::find_if(open_files.begin(), open_files.end(), [handle](const std::pair<AVPackageReader* const, TasofroFile>& it) {
		return &it.first->file_reader->handle == handle;
	});
	if (it == open_files.end()) {
		return 1;
	}

	return it->second.replace_ReadFile(regs,
		[](TasofroFile *fr, BYTE *buffer, DWORD size) {
			// Make sure we use the old size, which is part of the xor
			if (fr->pre_json_size > size) {
				return;
			}
			th175_crypt_file(buffer, fr->pre_json_size);
		},
		[&it](TasofroFile *fr, BYTE *buffer, DWORD size) {
			// Make sure we use the game reader's size, which will be used when the game decrypts its file
			if (it->first->file_size > size) {
				return;
			}
			th175_crypt_file(buffer, static_cast<size_t>(it->first->file_size));
		}
	);
}

extern "C" int BP_th175_close_file(x86_reg_t *regs, json_t *bp_info)
{
	AVPackageReader *reader = (AVPackageReader*)json_object_get_immediate(bp_info, regs, "file_reader");

	std::scoped_lock<std::mutex> lock(open_files_mutex);
	auto it = open_files.find(reader);
	if (it == open_files.end()) {
		return 1;
	}

	open_files.erase(it);
	return 1;
}
