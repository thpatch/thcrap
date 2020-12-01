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
#include "crypt.h"
#include "files_list.h"

struct file_desc_t
{
	uint64_t hash;
	uint64_t offset;
	uint64_t size;
};

int th175_init()
{
	ICrypt::instance = new CryptTh175();

	json_t *fileslist = stack_game_json_resolve("fileslist.js", nullptr);
	LoadFileNameListFromJson(fileslist);
	json_decref(fileslist);

	return 0;
}

extern "C" int BP_th175_file_desc(x86_reg_t *regs, json_t *bp_info)
{
	file_desc_t *desc = (file_desc_t*)json_object_get_immediate(bp_info, regs, "desc");

	Th135File *fr = hash_to_Th135File((DWORD)desc->hash);
	if (fr == nullptr) {
		return 1;
	}

	fr->init(fr->path.generic_u8string().c_str());
	if (fr->rep_buffer == nullptr) {
		fr->clear();
		return 1;
	}

	desc->size = fr->pre_json_size;
	return 1;
}

extern "C" int BP_th175_read_file(x86_reg_t *regs, json_t *bp_info)
{
	DWORD hash = json_object_get_immediate(bp_info, regs, "hash");
	char *buffer = (char*)json_object_get_immediate(bp_info, regs, "buffer");
	size_t offset = json_object_get_immediate(bp_info, regs, "offset");
	size_t size = json_object_get_immediate(bp_info, regs, "size");

	Th135File *fr = hash_to_Th135File(hash);
	if (fr == nullptr) {
		return 1;
	}

	if (fr->rep_buffer == nullptr) {
		return 1;
	}

	memset(buffer, 0, size);
	if (offset < fr->pre_json_size) {
		memcpy(buffer, (char*)fr->rep_buffer + offset, MAX(size, fr->pre_json_size - offset));
	}

	return 1;
}
