/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Tasofro plugins patching.
  */

#pragma once

#include <thcrap.h>
#include <jansson.h>

#ifdef __cplusplus
extern "C" {
#endif

int patch_dll(void *file_inout, size_t size_out, size_t size_in, json_t *patch);

#ifdef __cplusplus
}
#endif
