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
	BYTE data[0];
} tfcs_header_t;
#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif

	int patch_tfcs(BYTE *file_inout, size_t size_out, size_t size_in, json_t *patch);
	int patch_csv(BYTE *file_inout, size_t size_out, size_t size_in, json_t *patch);

#ifdef __cplusplus
}
#endif
