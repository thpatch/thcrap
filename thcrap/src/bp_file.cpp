/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoints for contiguous files.
  */

#include "thcrap.h"
#include <algorithm>

int file_rep_init(file_rep_t *fr, const char *file_name)
{
	if (fr->name) {
		file_rep_clear(fr);
	}
	fr->name = EnsureUTF8(file_name, strlen(file_name) + 1);
	fr->rep_buffer = stack_game_file_resolve(fr->name, &fr->pre_json_size);
	fr->hooks = patchhooks_build(fr->name);
	if (fr->hooks) {
		fr->patch = patchhooks_load_diff(fr->hooks, fr->name, &fr->patch_size);
	}
	fr->disable = false;
	return 1;
}

static int file_rep_hooks_run(file_rep_t *fr)
{
	return patchhooks_run(
		fr->hooks, fr->game_buffer, POST_JSON_SIZE(fr), fr->pre_json_size, fr->name, fr->patch
	);
}

int file_rep_clear(file_rep_t *fr)
{
	if (fr) {
		json_decref_safe(fr->patch);
		if (void* hooks = fr->hooks) {
			free(hooks);
		}
		if (char* name = fr->name) {
			free(name);
		}
		if (void* rep_buffer = fr->rep_buffer) {
			free(rep_buffer);
		}
		*fr = {}; // Sets all fields to 0 or NULL
		return 0;
	}
	return -1;
}

/// Thread-local storage
/// --------------------
THREAD_LOCAL(file_rep_t, fr_tls, NULL, file_rep_clear);
/// --------------------

/// Replace a file loaded entirely in memory
/// ----------------------------------------
int BP_file_buffer(x86_reg_t *regs, json_t *bp_info)
{
	file_rep_t *fr = fr_tls_get();
	if unexpected(fr->disable) {
		return 1;
	}

	// Parameters
	// ----------
	auto file_buffer = (BYTE**)json_object_get_pointer(bp_info, regs, "file_buffer");
	// ----------
	if(file_buffer) {
		fr->game_buffer = *file_buffer;
	}
	return 1;
}

int BP_file_load(x86_reg_t *regs, json_t *bp_info)
{
	file_rep_t *fr = fr_tls_get();
	if unexpected(fr->disable) {
		return 1;
	}


	// Mandatory parameters
	// --------------------
	BP_file_buffer(regs, bp_info);
	if (auto file_name = (char**)json_object_get_pointer(bp_info, regs, "file_name")) {
		file_rep_init(fr, *file_name);
	}
	auto file_size = json_object_get_pointer(bp_info, regs, "file_size");
	// -----------------

	// th08 and th09 use their file size variable as the loop counter for LZSS
	// decompression. Putting anything other than the original file size from
	// the archive there (by writing to that variable) will result in a few
	// bytes of corruption at the end of the decompressed file.
	// Therefore, these games need the POST_JSON_SIZE to be unconditionally
	// written out to registers three separate times.

	// However, we *do* check whether we have a file name. If we don't, we
	// can't possibly have resolved a replacement file that would give us a
	// custom file size.
	// This allows this breakpoint to be placed in front of memory allocation
	// calls that are used for more than just replaceable files, without
	// affecting unrelated memory allocations.
	if(file_size && fr->name) {
		if(!fr->pre_json_size) {
			fr->pre_json_size = *file_size;
		}
		*file_size = POST_JSON_SIZE(fr);
	}

	// Got everything for a full file replacement?
	if(!fr->rep_buffer || !fr->pre_json_size || !fr->game_buffer) {
		return 1;
	}

	// Let's do it
	memcpy(fr->game_buffer, fr->rep_buffer, fr->pre_json_size);

	file_rep_hooks_run(fr);

	// Post-load parameters
	// ------------------------
	if(size_t eip_jump_dist = json_object_get_hex(bp_info, "eip_jump_dist")) {
		regs->retaddr += eip_jump_dist;
	}
	if(void** file_buffer_addr_copy = (void**)json_object_get_pointer(bp_info, regs, "file_buffer_addr_copy")) {
		*file_buffer_addr_copy = fr->game_buffer;
	}
	// ------------------------

	file_rep_clear(fr);
	return 0;
}

// Cool function name.
int DumpDatFile(const char *dir, const char *name, const void *buffer, size_t size, bool overwrite_existing)
{
	if unexpected(!buffer || !name) {
		return -1;
	}

	BUILD_VLA_STR(char, fn, dir, "/", name);
	if (overwrite_existing || !PathFileExistsU(fn)) {
		file_write(fn, buffer, size);
	}
	VLA_FREE(fn);

	return 0;
}

int BP_file_loaded(x86_reg_t *regs, json_t *bp_info)
{
	file_rep_t *fr = fr_tls_get();
	if unexpected(fr->disable) {
		return 1;
	}

	const char *dat_dump;

	// Other breakpoints
	// -----------------
	BP_file_buffer(regs, bp_info);
	// -----------------

	if(!fr->game_buffer) {
		return 1;
	}
	dat_dump = runconfig_dat_dump_get();
	if(dat_dump) {
		DumpDatFile(dat_dump, fr->name, fr->game_buffer, fr->pre_json_size, false);
	}

	file_rep_hooks_run(fr);
	file_rep_clear(fr);
	return 1;
}
