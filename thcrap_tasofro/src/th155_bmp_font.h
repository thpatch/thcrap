/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Convert PNG+jdiff fonts to the bmp+footer format required by th155.
  */

#pragma once

#include <jansson.h>

size_t get_image_data_size(const char *fn, bool fill_alpha_for_24bpp);

size_t get_bmp_font_size(const char *fn, json_t *patch, size_t patch_size);
int patch_bmp_font(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch);
