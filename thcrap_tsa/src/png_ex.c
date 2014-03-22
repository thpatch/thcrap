/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * libpng extensions.
  */

#pragma once

#include <thcrap.h>
#include <png.h>
#include "png_ex.h"

int png_image_new(
	png_image_exp image,
	const png_uint_32 w,
	const png_uint_32 h,
	const png_uint_32 format
)
{
	if(image) {
		size_t image_size;
		SAFE_FREE(image->buf);
		png_image_free(&image->img);
		ZeroMemory(&image->img, sizeof(png_image));

		image->img.version = PNG_IMAGE_VERSION;
		image->img.width = w;
		image->img.height = h;
		image->img.format = format;

		image_size = PNG_IMAGE_SIZE(image->img);
		image->buf = malloc(image_size);
		if(image->buf) {
			ZeroMemory(image->buf, image_size);
		}
		return image->buf != NULL;
	}
	return -1;
}
