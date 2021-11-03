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
	patchhook_register("*.pl", patch_th175_pl, nullptr);
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
	void* vtable;
	DWORD refcount; // from IUnknown
	uint32_t unk_08;
	uint32_t unk_0C;
	uint32_t file_offset;
	uint32_t unk_14;
	uint32_t file_size;
	uint32_t unk_1c;
	// Game does SetFilePointer(..., file_offset + file_segment_offset, ...)
	uint32_t file_segment_offset;
	uint32_t unk_24;
	uint32_t unk_28;

	AVFileIO_DefaultReader* file_reader;
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

void do_partial_xor(uint8_t* dst, uint8_t* src, size_t size)
{
	for (size_t i = 0; i < size; i++) {
		dst[i] ^= src[i];
	}
}

// More readable version of the decrypt code.
// This one also doesn't require the input buffer size to be divisible by 4
uint32_t do_decrypt_step(uint32_t key)
{
	int64_t a = key * 0x5E4789C9ll;
	uint32_t b = (a >> 0x2E) + (a >> 0x3F);
	uint32_t ret = (key - b * 0xADC8) * 0xBC8F + b * 0xFFFFF2B9;
	if ((int32_t)ret <= 0) {
		ret += 0x7FFFFFFF;
	}
	return ret;
}

void th175_crypt_file(uint8_t* buffer, size_t size, size_t offset_in_file)
{
	size_t bytes_processed = 0;
	// File key, derived from the file's size and its offset in the archive file.
	uint32_t file_key = size ^ offset_in_file;

	for (size_t pos = 0; pos < size; pos += 4) {
		uint32_t xor = 0;
		uint32_t tmp_key = file_key;
		for (size_t i = 0; i < 4; i++) {
			tmp_key = do_decrypt_step(tmp_key);
			xor = (xor << 8) | (tmp_key & 0xFF);
		}

		if (pos + 4 <= size) {
			*(uint32_t*)(buffer + pos) ^= xor;
			bytes_processed += 4;
		}
		else {
			do_partial_xor(buffer + pos, (uint8_t*)&xor, size - pos);
			bytes_processed += size - pos;
		}
		file_key++;
	}

	assert(bytes_processed == size);
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
		[&it](TasofroFile *fr, BYTE *buffer, DWORD size) {
			// Make sure we use the old size, which is part of the xor
			if (fr->pre_json_size > size) {
				return;
			}
			th175_crypt_file(buffer, fr->pre_json_size, it->second.offset);
		},
		[&it](TasofroFile *fr, BYTE *buffer, DWORD size) {
			// Make sure we use the game reader's size, which will be used when the game decrypts its file
			if (it->first->file_size > size) {
				return;
			}
			th175_crypt_file(buffer, static_cast<size_t>(it->first->file_size), it->first->file_offset);
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
