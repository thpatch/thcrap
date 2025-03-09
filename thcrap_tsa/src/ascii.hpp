/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Helper functions for text rendering using the ascii.png sprites.
  */

#pragma once

float ascii_char_width();
float ascii_extent(const char *str);
float ascii_align_center(float x, const char *str);
float ascii_align_right(float x, const char *str);

// The game's non-variadic ASCII printing function.
//
// Or rather, a wrapper around it that we binhacked into the exectuable. The
// original function is a class method and therefore uses the __thiscall
// convention. Which is non-portable, so using it directly would at best lock
// us into MSVC… and at worst actually be counter-productive, as ZUN can
// change the function signature at any time and introduce additional
// parameters we don't care about.
// This workaround is meant to allow us to call the original function multiple
// times from within a single call to the variadic one, allowing us to split
// the incoming format string and draw substrings at different positions and
// with different ASCII state parameters.
// The return value is handed back directly to ascii_vpatchf().
typedef int TH_CDECL ascii_put_func_t(void *classptr, vector3_t &pos, const char *str);

// Main ASCII patching function, internally branching depending on the
// currently running game, and calling out to [putfunc] for every string to be
// rendered. Returns the return value of [putfunc].
extern "C" int TH_STDCALL ascii_vpatchf(
	ascii_put_func_t &putfunc, vector3_t &pos, const char* fmt, va_list va
);

/*
 * Points the ASCII module to the game's own state. Should be placed after
 * the game has allocated all of these.
 *
 * Own JSON parameters
 * -------------------
 *	[ClassPtr]
 *		Pointer to the beginning of the game's ASCII manager class.
 *		All parameters prefixed with . are offsets from this value.
 *		Required for [ascii_vpatchf].
 *		Type: immediate
 *
 *	[.Scale]
 *		Scale factor for newly created ASCII string instances.
 *		Type: immediate to vector2_t
 *
 *	[CharWidth]
 *		Space from one character to the next. Usually hardcoded and
 *		*different* from the width of the ascii.png sprites!
 *		Type: real or integer
 *
 * Other breakpoints called
 * ------------------------
 *	None
 */
extern "C" TH_EXPORT size_t BP_ascii_params(x86_reg_t *regs, json_t *bp_info);
