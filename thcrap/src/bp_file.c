/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoints for contiguous files.
  */

#include "thcrap.h"
#include "bp_file.h"

// For now, this should be enough; needs to be changed as soon as one game
// requires thread-safety!
static file_rep_t fr;

int BP_file_name(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	char **file_name = (char**)json_object_get_register(bp_info, regs, "file_name");
	// ----------

	size_t fn_len;

	if(!file_name) {
		return 1;
	}
	if(fr.name) {
		file_rep_clear(&fr);
	}

	fn_len = strlen(*file_name) + 1;

	{
		// Make a copy of the file name, ensuring UTF-8 in the process
		VLA(wchar_t, fn_w, fn_len);
		StringToUTF16(fn_w, *file_name, fn_len);

		fr.name = (char*)malloc(fn_len * UTF8_MUL * sizeof(char));
		StringToUTF8(fr.name, fn_w, fn_len);
		VLA_FREE(fn_w);
	}

	fr.rep_buffer = stack_gamefile_resolve(fr.name, &fr.rep_size);
	fr.hooks = patchhooks_build(fr.name);
	if(fr.hooks) {
		const char *game_id;
		size_t patch_fn_len = 0;
		size_t patch_size = 0;

		game_id = json_object_get_string(run_cfg, "game");
		if(game_id) {
			patch_fn_len += strlen(game_id) + 1;
		}
		patch_fn_len += fn_len + strlen(".jdiff") + 1;

		{
			VLA(char, patch_fn, patch_fn_len);
			patch_fn[0] = 0;	// Because strcat

			if(game_id) {
				strcpy(patch_fn, game_id);
				PathAddBackslashA(patch_fn);
			}
			strcat(patch_fn, fr.name);
			strcat(patch_fn, ".jdiff");

			fr.patch = stack_json_resolve(patch_fn, &patch_size);
			VLA_FREE(patch_fn);
		}
		fr.rep_size += patch_size;
	}
	// Print the type of the file
	if(fr.patch) {
		log_printf("(Patch) ");
	} else if(fr.rep_buffer && fr.rep_size) {
		log_printf("(External) ");
	} else {
		log_printf("(Archive) ");
	}
	if(fr.name) {
		log_printf("Loading %s ", fr.name);
	}
	return 1;
}

int BP_file_size(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	size_t *file_size = json_object_get_register(bp_info, regs, "file_size");
	int set_patch_size = !(json_is_false(json_object_get(bp_info, "set_patch_size")));
	// ----------

	// Other breakpoints
	// -----------------
	BP_file_name(regs, bp_info);
	// -----------------

	if(!file_size) {
		return 1;
	}

	fr.game_size = *file_size;

	if(fr.patch) {
		// If we only have a patch and no replacement file, the replacement size
		// doesn't yet include the actual base file part
		if(!fr.rep_buffer) {
			fr.rep_size += fr.game_size;
		}
		if(set_patch_size) {
			*file_size = fr.rep_size;
		}
	} else if(fr.rep_buffer && fr.rep_size) {
		// Set size of replacement file
		*file_size = fr.rep_size;
	}
	if(fr.name) {
		log_printf("(%d bytes)...\n", fr.rep_size ? fr.rep_size : *file_size);
	}
	return 1;
}

int BP_file_size_patch(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	size_t *file_size = json_object_get_register(bp_info, regs, "file_size");
	// ----------
	if(file_size && fr.rep_size && fr.game_size != fr.rep_size) {
		*file_size = fr.rep_size;
	}
	return 1;
}

int BP_file_buffer(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	BYTE **file_buffer;
	// ----------
	file_buffer = (BYTE**)json_object_get_register(bp_info, regs, "file_buffer");
	if(file_buffer) {
		fr.game_buffer = *file_buffer;
	}
	return 1;
}

int BP_file_load(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	size_t *file_buffer_addr_copy = json_object_get_register(bp_info, regs, "file_buffer_addr_copy");
	size_t stack_clear_size = json_object_get_hex(bp_info, "stack_clear_size");
	size_t eip_jump_dist = json_object_get_hex(bp_info, "eip_jump_dist");
	// ----------

	// Other breakpoints
	// -----------------
	BP_file_buffer(regs, bp_info);
	// -----------------

	if(!fr.game_buffer || !fr.rep_buffer || !fr.rep_size) {
		return 1;
	}

	// Let's do it
	memcpy(fr.game_buffer, fr.rep_buffer, fr.rep_size);

	patchhooks_run(fr.hooks, fr.game_buffer, fr.rep_size, fr.game_size, fr.patch, run_cfg);
	
	if(eip_jump_dist) {
		regs->retaddr += eip_jump_dist;
	}
	if(file_buffer_addr_copy) {
		*file_buffer_addr_copy = (size_t)fr.game_buffer;
	}
	if(stack_clear_size) {
		regs->esp += stack_clear_size;
	}
	file_rep_clear(&fr);
	return 0;
}

// Cool function name.
int DumpDatFile(const char *dir, const file_rep_t *fr)
{
	if(!fr || !fr->game_buffer) {
		return -1;
	}
	if(!dir) {
		dir = "dat";
	}
	{
		HANDLE hFile;

		size_t fn_len = strlen(dir) + 1 + strlen(fr->name) + 1;
		VLA(char, fn, fn_len);

		sprintf(fn, "%s/%s", dir, fr->name);
		CreateDirectory(fn, NULL);

		hFile = CreateFile(
			fn, GENERIC_WRITE, 0, NULL, CREATE_NEW, 
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL
		);
		if(hFile != INVALID_HANDLE_VALUE) {
			DWORD byte_ret;
			WriteFile(hFile, fr->game_buffer, fr->game_size, &byte_ret, NULL);
			CloseHandle(hFile);
		}
		VLA_FREE(fn);
	}
	return 0;
}

int BP_file_loaded(x86_reg_t *regs, json_t *bp_info)
{
	json_t *dat_dump;

	// Other breakpoints
	// -----------------
	BP_file_buffer(regs, bp_info);
	// -----------------

	if(!fr.game_buffer) {
		return 1;
	}
	dat_dump = json_object_get(run_cfg, "dat_dump");
	if(!json_is_false(dat_dump)) {
		DumpDatFile(json_string_value(dat_dump), &fr);
	}
	
	patchhooks_run(fr.hooks, fr.game_buffer, fr.rep_size, fr.game_size, fr.patch, run_cfg);

	file_rep_clear(&fr);
	return 1;
}

int file_rep_clear(file_rep_t *fr)
{
	SAFE_FREE(fr->rep_buffer);
	json_decref(fr->patch);
	fr->patch = NULL;
	json_decref(fr->hooks);
	fr->hooks = NULL;
	fr->rep_size = 0;
	fr->game_size = 0;
	fr->object = NULL;
	SAFE_FREE(fr->name);
	return 1;
}
