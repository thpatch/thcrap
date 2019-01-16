/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * GDI-based text rendering layout.
  */

#pragma once

#ifdef __cplusplus
Option<HFONT> font_block_get(int id);

extern "C" {
#endif

/// Tokenization
/// ------------
// Split the string into an array of tokens to render in a sequence.
// These are either strings (= direct text)
// or arrays in itself (= layout commands).
json_t* layout_tokenize(const char *str, size_t len);
/// ------------

// Text layout wrapper
BOOL WINAPI layout_TextOutU(
	HDC hdc,
	int orig_x,
	int orig_y,
	LPCSTR lpString,
	int c
);

// Raw text width calculation, without taking layout markup into account.
size_t GetTextExtentBase(HDC hdc, const json_t *str_obj);

/// Internal, full-size text extent functions
/// -----------------------------------------
size_t __stdcall text_extent_full(const char *str);
size_t __stdcall text_extent_full_for_font(const char *str, HFONT font);
/// -----------------------------------------

/// Public, half-size text extent functions. Actively used by breakpoints!
/// ----------------------------------------------------------------------
// Calculates the rendered length of [str] on the current text DC
// with the currently selected font, taking layout markup into account.
size_t __stdcall GetTextExtent(const char *str);

// GetTextExtent with an additional font parameter.
// [font] is temporarily selected into the DC for the length calcuation.
size_t __stdcall GetTextExtentForFont(const char *str, HFONT font);

// GetTextExtentForFont with the font pointer pulled from the TSA font block.
size_t __stdcall GetTextExtentForFontID(const char *str, size_t id);
/// ----------------------------------------------------------------------

// Calculates the half-size X offset at which to display [ruby] to make it
// appear centered above the text [bottom], which comes after [begin]:
//
//	<-ret.->[ruby]
//	[begin][bottom]
size_t ruby_offset_half(
	const char *begin, const char *bottom, const char *ruby,
	HFONT font_bottom, HFONT font_ruby
);

/*
 * Returns the widest string in an array of strings. widest_string returns
 * integers in [width], widest_string_f returns floats.
 *
 * Own JSON parameters
 * -------------------
 *	[font_id]
 *		Font to be used for the width calculation
 *		Type: immediate (unsigned integer)
 *
 *	[strs]
 *		Strings to calculate the width of
 *		Type: flexible array of pointers
 *
 *	[width]
 *		Target address for the calculation
 *		Type: pointer
 *
 *	[correction_summand] (optional)
 *		Additional value to add to the final result
 *		Type: immediate (signed integer)
 *
 * Other breakpoints called
 * ------------------------
 *	None
*/
__declspec(dllexport) int BP_widest_string(x86_reg_t *regs, json_t *bp_info);
__declspec(dllexport) int BP_widest_string_f(x86_reg_t *regs, json_t *bp_info);

int layout_mod_init(HMODULE hMod);
void layout_mod_detour(void);
void layout_mod_exit(void);

#ifdef __cplusplus
}
#endif
