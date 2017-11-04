/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * PNG files reading.
  */

#pragma once

#include <thcrap.h>

#ifdef __cplusplus
extern "C" {
#endif

// Reads the PNG file [fn]. The returned value is an array of lines. It has to be freed by the caller.
// If this function is successful, bpp will be either 24 or 32.
BYTE **png_image_read(const char *fn, uint32_t *width, uint32_t *height, uint8_t *bpp);

#ifdef __cplusplus
}
#endif
