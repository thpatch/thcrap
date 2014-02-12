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

int file_rep_clear(file_rep_t *fr)
{
	if(!fr) {
		return -1;
	}
	SAFE_FREE(fr->rep_buffer);
	fr->patch = json_decref_safe(fr->patch);
	fr->hooks = json_decref_safe(fr->hooks);
	fr->rep_size = 0;
	fr->game_size = 0;
	fr->object = NULL;
	SAFE_FREE(fr->name);
	return 0;
}

/// Thread-local storage
/// --------------------
// TLS index of the global file_rep_t object.
// (Yeah, we *could* use __declspec(thread)... if we didn't have to accommodate
// for Windows XP users. See http://www.nynaeve.net/?p=187.)
static DWORD fr_tls = 0xffffffff;

file_rep_t* fr_tls_get(void)
{
	file_rep_t *fr = (file_rep_t*)TlsGetValue(fr_tls);
	if(!fr) {
		fr = (file_rep_t*)malloc(sizeof(file_rep_t));
		if(fr) {
			ZeroMemory(fr, sizeof(file_rep_t));
		}
		TlsSetValue(fr_tls, fr);
	}
	return fr;
}

void fr_tls_free(void)
{
	file_rep_t *fr = (file_rep_t*)TlsGetValue(fr_tls);
	if(fr) {
		file_rep_clear(fr);
		SAFE_FREE(fr);
		TlsSetValue(fr_tls, fr);
	}
}
/// --------------------

int BP_file_name(x86_reg_t *regs, json_t *bp_info)
{
	file_rep_t *fr = fr_tls_get();

	size_t fn_len;

	// Parameters
	// ----------
	char **file_name = (char**)json_object_get_register(bp_info, regs, "file_name");
	// ----------

	if(!file_name) {
		return 1;
	}
	if(fr->name) {
		file_rep_clear(fr);
	}
	fn_len = strlen(*file_name) + 1;
	fr->name = EnsureUTF8(*file_name, fn_len);
	fr->rep_buffer = stack_game_file_resolve(fr->name, &fr->rep_size);
	fr->hooks = patchhooks_build(fr->name);
	if(fr->hooks) {
		size_t diff_fn_len = fn_len + strlen(".jdiff") + 1;
		size_t diff_size = 0;
		{
			VLA(char, diff_fn, diff_fn_len);
			strcpy(diff_fn, fr->name);
			strcat(diff_fn, ".jdiff");
			fr->patch = stack_game_json_resolve(diff_fn, &diff_size);
			VLA_FREE(diff_fn);
		}
		fr->rep_size += diff_size;
	}
	return 1;
}

int BP_file_size(x86_reg_t *regs, json_t *bp_info)
{
	file_rep_t *fr = fr_tls_get();

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

	fr->game_size = *file_size;

	if(fr->patch) {
		// If we only have a patch and no replacement file, the replacement size
		// doesn't yet include the actual base file part
		if(!fr->rep_buffer) {
			fr->rep_size += fr->game_size;
		}
		if(set_patch_size) {
			*file_size = fr->rep_size;
		}
	} else if(fr->rep_buffer && fr->rep_size) {
		// Set size of replacement file
		*file_size = fr->rep_size;
	}
	return 1;
}

int BP_file_size_patch(x86_reg_t *regs, json_t *bp_info)
{
	file_rep_t *fr = fr_tls_get();

	// Parameters
	// ----------
	size_t *file_size = json_object_get_register(bp_info, regs, "file_size");
	// ----------
	if(file_size && fr->rep_size && fr->game_size != fr->rep_size) {
		*file_size = fr->rep_size;
	}
	return 1;
}

int BP_file_buffer(x86_reg_t *regs, json_t *bp_info)
{
	file_rep_t *fr = fr_tls_get();

	// Parameters
	// ----------
	BYTE **file_buffer = (BYTE**)json_object_get_register(bp_info, regs, "file_buffer");
	// ----------
	if(file_buffer) {
		fr->game_buffer = *file_buffer;
	}
	return 1;
}

int BP_file_load(x86_reg_t *regs, json_t *bp_info)
{
	file_rep_t *fr = fr_tls_get();

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

	if(!fr->game_buffer || !fr->rep_buffer || !fr->rep_size) {
		return 1;
	}

	// Let's do it
	memcpy(fr->game_buffer, fr->rep_buffer, fr->rep_size);

	patchhooks_run(fr->hooks, fr->game_buffer, fr->rep_size, fr->game_size, fr->patch, run_cfg);

	if(eip_jump_dist) {
		regs->retaddr += eip_jump_dist;
	}
	if(file_buffer_addr_copy) {
		*file_buffer_addr_copy = (size_t)fr->game_buffer;
	}
	if(stack_clear_size) {
		regs->esp += stack_clear_size;
	}
	fr_tls_free();
	return 0;
}

// Cool function name.
int DumpDatFile(const char *dir, const file_rep_t *fr)
{
	if(!fr || !fr->game_buffer || !fr->name) {
		return -1;
	}
	if(!dir) {
		dir = "dat";
	}
	{
		size_t fn_len = strlen(dir) + 1 + strlen(fr->name) + 1;
		VLA(char, fn, fn_len);

		sprintf(fn, "%s/%s", dir, fr->name);

		if(!PathFileExists(fn)) {
			file_write(fn, fr->game_buffer, fr->game_size);
		}
		VLA_FREE(fn);
	}
	return 0;
}

int BP_file_loaded(x86_reg_t *regs, json_t *bp_info)
{
	file_rep_t *fr = fr_tls_get();
	json_t *dat_dump;

	// Other breakpoints
	// -----------------
	BP_file_buffer(regs, bp_info);
	// -----------------

	if(!fr->game_buffer) {
		return 1;
	}
	dat_dump = json_object_get(run_cfg, "dat_dump");
	if(!json_is_false(dat_dump)) {
		DumpDatFile(json_string_value(dat_dump), fr);
	}

	patchhooks_run(fr->hooks, fr->game_buffer, fr->rep_size, fr->game_size, fr->patch, run_cfg);

	fr_tls_free();
	return 1;
}

int bp_file_init(void)
{
	fr_tls = TlsAlloc();
	return 0;
}

int bp_file_exit(void)
{
	return TlsFree(fr_tls);
}
