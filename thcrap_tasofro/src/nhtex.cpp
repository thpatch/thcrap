/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Patching of nhtex files.
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"

size_t get_nhtex_size(const char *fn, json_t*, size_t)
{
	char rep_fn[MAX_PATH];
	char *rep_fn_end;
	void *rep_file;
	size_t rep_size;

	strcpy(rep_fn, fn);
	rep_fn_end = rep_fn + strlen(rep_fn);

	strcpy(rep_fn_end, ".png");
	rep_file = stack_game_file_resolve(rep_fn, &rep_size);
	if (rep_file) {
		free(rep_file);
		return rep_size;
	}

	strcpy(rep_fn_end, ".dds");
	rep_file = stack_game_file_resolve(rep_fn, &rep_size);
	if (rep_file) {
		free(rep_file);
		return rep_size;
	}

	return 0;
}

int patch_nhtex(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t*)
{
	DWORD *file_header = (DWORD*)file_inout;
	DWORD *file_size = &file_header[6];
	BYTE *file_content = (BYTE*)file_inout + 0x30;

	if (*file_size + 0x30 != size_in)
		log_printf("Warning: nhtex: file size and header size don't match.\n");

	char rep_fn[MAX_PATH];
	char *rep_fn_end;
	void *rep_file;
	size_t rep_size;

	strcpy(rep_fn, fn);
	rep_fn_end = rep_fn + strlen(rep_fn);

	strcpy(rep_fn_end, ".png");
	rep_file = stack_game_file_resolve(rep_fn, &rep_size);
	if (rep_file) {
		if (rep_size + 0x30 > size_out) {
			log_printf("Error: nhtex: not enough room for replacement file.\n");
			return -1;
		}
		memcpy(file_content, rep_file, rep_size);
		*file_size = rep_size;
		free(rep_file);
		return 1;
	}

	strcpy(rep_fn_end, ".dds");
	rep_file = stack_game_file_resolve(rep_fn, &rep_size);
	if (rep_file) {
		if (rep_size + 0x30 > size_out) {
			log_printf("Error: nhtex: not enough room for replacement file.\n");
			return -1;
		}
		memcpy(file_content, rep_file, rep_size);
		*file_size = rep_size;
		free(rep_file);
		return 1;
	}

	return 0;
}
