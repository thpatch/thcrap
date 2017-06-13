/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Win32 detour function level promotion.
  */

#pragma once

/**
  * This module eliminates the redundancy in detouring certain Win32 function
  * families - sets of functions that act as wrappers of decreasing levels
  * around one functionality.
  *
  * This is done by detouring each individual wrapper with a function that
  * merely converts and passes each level's parameters to the next one
  * ("promoting"), terminating in an invocation of the detour chain for the
  * lowest-level function.
  *
  * Other modules would then only have to detour a single function - the
  * lowest-level one - to catch all calls to the whole family of functions,
  * instead of detouring every function and duplicating all the promotion
  * code required.
  *
  * For example, the CreateFont() family consists of three functions:
  * CreateFont(), CreateFontIndirect() and CreateFontIndirectEx(). With the
  * promotion module, it's enough to detour CreateFontIndirectEx() to catch
  * all three functions.
  */

/// CreateFont() family (promoting to CreateFontIndirectExA)
/// -------------------
HFONT WINAPI promote_CreateFontA(
	int cHeight,
	int cWidth,
	int cEscapement,
	int cOrientation,
	int cWeight,
	DWORD bItalic,
	DWORD bUnderline,
	DWORD bStrikeOut,
	DWORD iCharSet,
	DWORD iOutPrecision,
	DWORD iClipPrecision,
	DWORD iQuality,
	DWORD iPitchAndFamily,
	LPCSTR pszFaceName
);

HFONT WINAPI promote_CreateFontIndirectA(
	CONST LOGFONTA *lplf
);
/// -------------------

// _mod_init instead of _mod_detour because we want these to be as low as
// possible... Kinda ugly, actually.
void promote_mod_init(void);
