/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * ANM structures and function declarations.
  */

#pragma once

/// TSA types
/// ---------
// All of the 16-bit formats are little-endian.
typedef enum {
	FORMAT_BGRA8888 = 1,
	FORMAT_RGB565 = 3,
	FORMAT_ARGB4444 = 5, // 0xGB 0xAR
	FORMAT_GRAY8 = 7
} format_t;

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
/// ---------

/// Patching types
/// --------------
// Why couldn't libpng just include the buffer pointer into the png_image
// structure. Are different pointer sizes (thus, differing structure sizes)
// really that bad?
typedef struct {
	png_image img;
	png_bytep buf;
} png_image_ex, *png_image_exp;

// Alpha analysis results
typedef enum {
	SPRITE_ALPHA_EMPTY,
	SPRITE_ALPHA_OPAQUE,
	SPRITE_ALPHA_FULL
} sprite_alpha_t;

// Coordinates for sprite-based patching
typedef struct {
	// General info
	int bpp;
	format_t format;

	// Coordinates for the sprite inside the THTX
	png_uint_32 dst_x;
	png_uint_32 dst_y;
	png_uint_32 dst_stride;
	png_bytep dst_buf;

	// Coordinates for the replacement inside the PNG
	png_uint_32 rep_x;
	png_uint_32 rep_y;
	png_uint_32 rep_stride;
	png_bytep rep_buf;

	// Size of the rectangle to copy. Clamped to match the dimensions of the
	// PNG replacement in case it's smaller than the sprite.
	png_uint_32 copy_w;
	png_uint_32 copy_h;
} sprite_patch_t;
/// --------------

/// Formats
/// -------
unsigned int format_Bpp(format_t format);
unsigned int format_png_equiv(format_t format);

// Returns the maximum alpha value (representing 100% opacity) for [format].
png_byte format_alpha_max(format_t format);
// Returns the sum of the alpha values for a number of [pixels] starting at [data].
size_t format_alpha_sum(png_bytep data, unsigned int pixels, format_t format);

// Converts a number of BGRA8888 [pixels] in [data] to the given [format] in-place.
void format_from_bgra(png_bytep data, unsigned int pixels, format_t format);
/// -------

/// Sprite-level patching
/// ---------------------
// Calculates the coordinates.
int sprite_patch_set(
	sprite_patch_t *sp,
	const size_t thtx_x,
	const size_t thtx_y,
	thtx_header_t *thtx,
	const sprite_t *sprite,
	const png_image_exp image
);

// Analyzes the alpha channel contents in a rectangle of size [w]*[h] in the
// bitmap [buf]. [stride] is used to jump from line to line.
sprite_alpha_t sprite_alpha_analyze(
	const png_bytep buf,
	const format_t format,
	const size_t stride,
	const png_uint_32 w,
	const png_uint_32 h
);
// Convenience functions to analyze destination and replacement sprites
sprite_alpha_t sprite_alpha_analyze_rep(const sprite_patch_t *sp);
sprite_alpha_t sprite_alpha_analyze_dst(const sprite_patch_t *sp);

// Simply overwrites the destination sprite with its replacement.
int sprite_replace(const sprite_patch_t *sp);

// Performs alpha analysis on [sp] and runs an appropriate patching function.
sprite_alpha_t sprite_patch(const sprite_patch_t *sp);
/// ---------------------

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
