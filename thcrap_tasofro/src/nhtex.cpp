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

static TLSSlot tls;

size_t get_nhtex_size(const char *fn, json_t*, size_t)
{
	char rep_fn[MAX_PATH];

	size_t rep_fn_length = strlen(fn);

	memcpy(rep_fn, fn, rep_fn_length);

	char* rep_fn_end = rep_fn + rep_fn_length;

	memcpy(rep_fn_end, ".png", sizeof(".png"));

	size_t rep_size = 0;
	void* rep_file = stack_game_file_resolve(rep_fn, &rep_size);
	if (!rep_file) {
		memcpy(rep_fn_end, ".dds", sizeof(".dds"));
		rep_file = stack_game_file_resolve(rep_fn, &rep_size);
	}

	TlsSetValue(tls.slot, rep_file);
	return rep_size;
}

int patch_nhtex(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t*)
{
	DWORD *file_header = (DWORD*)file_inout;
	DWORD *file_size = &file_header[6];
	BYTE *file_content = (BYTE*)file_inout + 0x30;

	if (*file_size + 0x30 != size_in) {
		log_print("Warning: nhtex: file size and header size don't match.\n");
	}

	if (void* rep_file = TlsGetValue(tls.slot)) {
		size_t rep_size = size_out - size_in + 0x30;
		log_printf("file size: %zu (out %zu)\n", rep_size, size_out);
		if (rep_size > size_out) {
			log_print("Error: nhtex: not enough room for replacement file.\n");
			return -1;
		}
		memcpy(file_content, rep_file, rep_size);
		*file_size = rep_size;
		free(rep_file);
		return 1;
	}
	return 0;
}
