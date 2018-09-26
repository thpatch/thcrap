/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * ANM structures and function declarations.
  */

#pragma once

#include <thtypes/anm_types.h>

/// Patching types
/// --------------
typedef struct {
	png_uint_32 x;
	png_uint_32 y;
	png_uint_32 w;
	png_uint_32 h;
} sprite_local_t;

// Alpha analysis results
typedef enum {
	SPRITE_ALPHA_EMPTY,
	SPRITE_ALPHA_OPAQUE,
	SPRITE_ALPHA_FULL
} sprite_alpha_t;

// All ANM data we need
typedef struct {
	// X and Y offsets of the THTX inside the image
	png_uint_32 x;
	png_uint_32 y;

	// Offset to the next entry in the ANM archive. 0 indicates the last one.
	size_t nextoffset;

	// File name of the original PNG associated with the bitmap.
	const char *name;

	thtx_header_t *thtx;

	sprite_local_t *sprites;
	size_t sprite_num;
} anm_entry_t;

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
png_uint_32 format_png_equiv(format_t format);

// Returns the maximum alpha value (representing 100% opacity) for [format].
png_byte format_alpha_max(format_t format);
// Returns the sum of the alpha values for a number of [pixels] starting at [data].
size_t format_alpha_sum(png_bytep data, unsigned int pixels, format_t format);

// Converts a number of BGRA8888 [pixels] in [data] to the given [format] in-place.
void format_from_bgra(png_bytep data, unsigned int pixels, format_t format);

// Blitting operations
// -------------------
// These blit a row with length [pixels] from [rep] to [dst], using [format].
typedef void (*BlitFunc_t)(png_byte *dst, const png_byte *rep, unsigned int pixels, format_t format);

// Simply overwrites a number of [pixels] in [rep] with [dst].
void format_copy(png_byte *dst, const png_byte *rep, unsigned int pixels, format_t format);
// Alpha-blends a number of [pixels] from [rep] on top of [dst].
void format_blend(png_byte *dst, const png_byte *rep, unsigned int pixels, format_t format);
// -------------------
/// -------

/// ANM structure
/// -------------
// Fills [entry] with the data of an ANM entry starting at [in], automatically
// detecting the correct source format.
int anm_entry_init(anm_entry_t &entry, BYTE *in);

void anm_entry_clear(anm_entry_t &entry);
/// -------------

/// Sprite-level patching
/// ---------------------
// Splits a [sprite] that wraps around the texture into two non-wrapping parts,
// adding the newly created sprites to the end of [entry.sprites].
int sprite_split_x(anm_entry_t &entry, sprite_local_t *sprite);
int sprite_split_y(anm_entry_t &entry, sprite_local_t *sprite);

// Calculates the coordinates.
int sprite_patch_set(
	sprite_patch_t &sp,
	const anm_entry_t &entry,
	const sprite_local_t *sprite,
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
sprite_alpha_t sprite_alpha_analyze_rep(const sprite_patch_t &sp);
sprite_alpha_t sprite_alpha_analyze_dst(const sprite_patch_t &sp);

// Runs the blitting function [func] on each row of [sp].
int sprite_blit(const sprite_patch_t &sp, const BlitFunc_t func);

// Performs alpha analysis on [sp] and runs an appropriate blitting function.
sprite_alpha_t sprite_patch(const sprite_patch_t &sp);

// Walks through the patch stack and patches every game-local PNG file called
// [entry->name] onto the THTX texture [entry->thtx] on sprite level, using
// the coordinates in [entry->sprites].
int stack_game_png_apply(anm_entry_t *entry);
/// ---------------------

/// Sprite boundary dumping
/// -----------------------
char* fn_for_bounds(const char *fn);

void bounds_init(png_image_ex &bounds, const thtx_header_t *thtx, const char *fn);

int bounds_draw_line(
	png_image_ex &image,
	const size_t x, const size_t y,
	const size_t pixeldist, const size_t len, png_byte val
);
int bounds_draw_hline(
	png_image_ex &image, const size_t x, const size_t y, const size_t len, png_byte val
);
int bounds_draw_vline(
	png_image_ex &image, const size_t x, const size_t y, const size_t len, png_byte val
);
int bounds_draw_rect(
	png_image_ex &image, const size_t thtx_x, const size_t thtx_y, const sprite_local_t *spr
);

int bounds_store(const char *fn, png_image_ex &image);
/// -----------------------
