/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Image patching for nsml engine.
  */

#pragma once

#include <jansson.h>

size_t get_image_data_size(const char *fn, bool fill_alpha_for_24bpp);

size_t get_cv2_size(const char *fn, json_t*, size_t);
size_t get_bmp_size(const char *fn, json_t*, size_t);

int patch_cv2(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t*);
int patch_bmp(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t*);
