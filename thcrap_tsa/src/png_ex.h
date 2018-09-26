/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * libpng extensions.
  */

#pragma once

#define PNG_FORMAT_INVALID (-1)

// Why couldn't libpng just include the buffer pointers into the png_image
// structure. Are different pointer sizes (thus, differing structure sizes)
// really that bad?
typedef struct {
	png_image img;
	png_bytep buf;
	png_bytep palette;
} png_image_ex, *png_image_exp;

// Creates a new, empty PNG image with the given [format] and dimensions.
// [image] must either be zeroed out or represent a valid PNG image.
// Returns 0 on success, 1 on allocation failure.
int png_image_new(
	png_image_ex &image,
	const png_uint_32 w,
	const png_uint_32 h,
	const png_uint_32 format
);

// Resizes [image] to the given new dimensions.
// Returns 0 on success, 1 on allocation failure.
int png_image_resize(
	png_image_ex &image,
	const size_t new_w,
	const size_t new_h
);

// Essentially a Unicode wrapper around png_image_write_to_file().
int png_image_store(const char *fn, png_image_ex &image);

int png_image_clear(png_image_ex &image);
