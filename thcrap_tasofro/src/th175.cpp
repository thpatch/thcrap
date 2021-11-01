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

#define IMUL_1(x) imul(&edx, &eax, eax, x);
#define IMUL_3(x, y, z) imul(NULL, &x, y, z)
void imul(uint32_t* dst_high, uint32_t* dst_low, uint32_t src_1, uint32_t src_2)
{
	int64_t ret = ((int64_t)(int32_t)src_1) * (int32_t)src_2;
	if (dst_high) {
		*dst_high = ret >> 32;
	}
	*dst_low = (uint32_t)ret;
}

void th175_crypt_file(uint8_t* buffer, size_t size, size_t offset_in_file)
{
	uint32_t* buffer_int_array = (uint32_t*)buffer;
	uint32_t eax, ebx, ecx, edx, /* ebp, */ esi, edi;

	// uint32_t param1 = (uint32_t)buffer;
	uint32_t param2 = 0;
	uint32_t param3 = size;
	// Param 4 is an object with the header size and the file size

	// eax = param4
	edx = param3;
	if (1) {
		ecx = size;
		ecx ^= offset_in_file; // from param4
		edx = param3 >> 2;
	}

	eax = param2 >> 2;
	// ebp = param1;
	ecx += eax;

	while (edx > 0) {
		uint32_t stack_1 = edx;
		eax = ecx;
		edx = 0x5E4789C9;
		esi = ecx;
		edi = 0x5E4789C9;

		// copy
		IMUL_1(edx);
		eax = edx;
		edx = (int32_t)edx >> 0x0E;
		eax >>= 0x1F;
		edx += eax;
		IMUL_3(eax, edx, 0xADC8);
		IMUL_3(edx, edx, 0xFFFFF2B9);
		esi -= eax;
		IMUL_3(eax, esi, 0xBC8F);
		ebx = eax + edx;
		esi = eax + edx + 0x7FFFFFFF;

		if ((int32_t)ebx > 0) {
			esi = ebx;
		}
		eax = esi;
		ebx = esi;

		// paste
		IMUL_1(edi); // edi instead of edx
		eax = edx;
		edx = (int32_t)edx >> 0x0E;
		eax >>= 0x1F;
		edx += eax;
		IMUL_3(eax, edx, 0xADC8);
		IMUL_3(edx, edx, 0xFFFFF2B9);
		ebx -= eax; // ebx instead of esi
		IMUL_3(eax, ebx, 0xBC8F); // same
		edi = eax + edx; // edi instead of ebx
		ebx = eax + edx + 0x7FFFFFFF; // ebx instead of edi

		edx = 0x5E4789C9;
		if ((int32_t)edi > 0) {
			ebx = edi;
		}

		esi <<= 8;
		eax = ebx;
		edi = ebx & 0xFF;

		// paste
		IMUL_1(edx); // edx instead of edi
		edi |= esi; // Added in the middle
		eax = edx;
		edx = (int32_t)edx >> 0x0E;
		eax >>= 0x1F;
		edx += eax;
		IMUL_3(eax, edx, 0xADC8);
		IMUL_3(edx, edx, 0xFFFFF2B9);
		ebx -= eax; // ebx instead of esi
		IMUL_3(eax, ebx, 0xBC8F); // same
		esi = eax + edx; // esi instead of ebx
		ebx = eax + edx + 0x7FFFFFFF; // ebx instead of edi

		edx = 0x5E4789C9;
		if ((int32_t)esi > 0) {
			ebx = esi;
		}

		edi <<= 8;
		eax = ebx;
		esi = ebx & 0xFF;

		// paste
		IMUL_1(edx); // edx instead of edi
		esi |= edi; // Added in the middle
		eax = edx;
		edx = (int32_t)edx >> 0x0E;
		eax >>= 0x1F;
		edx += eax;
		IMUL_3(eax, edx, 0xADC8);
		IMUL_3(edx, edx, 0xFFFFF2B9);
		ebx -= eax; // ebx instead of esi
		IMUL_3(eax, ebx, 0xBC8F); // same
		edi = eax + edx; // edi instead of ebx
		eax = eax + edx + 0xFF; // Completely different

		edx = stack_1;
		if ((int32_t)edi > 0) {
			eax = edi;
		}
		esi <<= 8;
		ecx++;
		eax &= 0xFF;
		eax |= esi;
		*buffer_int_array ^= eax;
		buffer_int_array++;
		edx--;
	}
}
#undef IMUL_1
#undef IMUL_3

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
			th175_crypt_file(buffer, fr->pre_json_size, it->first->file_offset + it->first->file_segment_offset);
		},
		[&it](TasofroFile *fr, BYTE *buffer, DWORD size) {
			// Make sure we use the game reader's size, which will be used when the game decrypts its file
			if (it->first->file_size > size) {
				return;
			}
			th175_crypt_file(buffer, static_cast<size_t>(it->first->file_size), it->first->file_offset + it->first->file_segment_offset);
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
