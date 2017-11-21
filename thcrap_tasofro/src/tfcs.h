/**
* Touhou Community Reliant Automatic Patcher
* Tasogare Frontier support plugin
*
* ----
*
* Patching of TFCS files.
*/

#pragma once

#include <thcrap.h>
#include "thcrap_tasofro.h"

#pragma pack(push, 1)
typedef struct {
	char magic[5];
	DWORD comp_size;
	DWORD uncomp_size;
	BYTE data[1];
} tfcs_header_t;
#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif

	int patch_tfcs(void *file_inout, size_t size_out, size_t size_in, const char*, json_t *patch);
	int patch_csv(char *file_inout, size_t size_out, size_t size_in, const char*, json_t *patch);
	size_t get_tfcs_size(const char*, json_t*, size_t patch_size);

#ifdef __cplusplus
}
#endif
