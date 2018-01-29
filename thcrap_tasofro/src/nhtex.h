/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Patching of nhtex files.
  */

#pragma once

#include <jansson.h>

size_t get_nhtex_size(const char *fn, json_t*, size_t);
int patch_nhtex(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t*);
