/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * ANM structures and function declarations.
  */

#pragma once

// Why couldn't libpng just include the buffer pointer into the png_image
// structure. Are different pointer sizes (thus, differing structure sizes)
// really that bad?
typedef struct {
	png_image img;
	png_bytep buf;
} png_image_ex, *png_image_exp;

/// Enums
/// -----
// All of the 16-bit formats are little-endian.
typedef enum {
	FORMAT_BGRA8888 = 1,
	FORMAT_RGB565 = 3,
	FORMAT_ARGB4444 = 5, // 0xGB 0xAR
	FORMAT_GRAY8 = 7
} format_t;
/// -----

/// Structures
/// ----------
typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
	char magic[4];
	WORD zero;
	WORD format;
	// These may be different from the parent entry.
	WORD w, h;
	DWORD size;
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
	unsigned char data[];
} PACK_ATTRIBUTE thtx_header_t;

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
	DWORD id;
	float x, y, w, h;
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE sprite_t;
/// ----------

/// Formats
/// -------
unsigned int format_Bpp(WORD format);
unsigned int format_png_equiv(WORD format);
void format_from_bgra(png_bytep data, unsigned int pixels, WORD format);
/// -------

/// Sprite boundary dumping
/// -----------------------
png_bytep bounds_init(png_image_exp bounds, thtx_header_t *thtx, const char *fn);
png_bytep bounds_resize(png_image_exp image, const size_t new_w, const size_t new_h);

char* fn_for_bounds(const char *fn);

int bounds_draw_line(
	png_image_exp image,
	const size_t x, const size_t y,
	const size_t pixeldist, const size_t len, png_byte val
);
int bounds_draw_hline(
	png_image_exp image, const size_t x, const size_t y, const size_t len, png_byte val
);
int bounds_draw_vline(
	png_image_exp image, const size_t x, const size_t y, const size_t len, png_byte val
);
int bounds_draw_rect(png_image_exp image, const size_t thtx_x, const size_t thtx_y, sprite_t *spr);

int bounds_store(const char *fn, png_image_exp image);
/// -----------------------
