/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Patching of ACT and NUT files.
  */

#pragma once

#include <thcrap.h>
#include "thcrap_tasofro.h"

#ifdef __cplusplus
extern "C" {
#endif

	int patch_act(void *file_inout, size_t size_out, size_t size_in, json_t *patch);
	int patch_nut(void *file_inout, size_t size_out, size_t size_in, json_t *patch);

#ifdef __cplusplus
}
#endif
