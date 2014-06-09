/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * gdi32.dll functions.
  */

#pragma once

/// Promotion wrappers
/// ------------------
typedef HFONT (WINAPI *CreateFontIndirectA_type)(CONST LOGFONTA*);
typedef HFONT (WINAPI *CreateFontIndirectExA_type)(CONST ENUMLOGFONTEXDVA*);

HFONT WINAPI lower_CreateFontA(
	__in CreateFontIndirectA_type down_func,
	__in int cHeight,
	__in int cWidth,
	__in int cEscapement,
	__in int cOrientation,
	__in int cWeight,
	__in DWORD bItalic,
	__in DWORD bUnderline,
	__in DWORD bStrikeOut,
	__in DWORD iCharSet,
	__in DWORD iOutPrecision,
	__in DWORD iClipPrecision,
	__in DWORD iQuality,
	__in DWORD iPitchAndFamily,
	__in_opt LPCSTR pszFaceName
);

HFONT WINAPI lower_CreateFontIndirectA(
	__in CreateFontIndirectExA_type down_func,
	__in CONST LOGFONTA *lplf
);
/// ------------------

HFONT WINAPI CreateFontU(
	__in int cHeight,
	__in int cWidth,
	__in int cEscapement,
	__in int cOrientation,
	__in int cWeight,
	__in DWORD bItalic,
	__in DWORD bUnderline,
	__in DWORD bStrikeOut,
	__in DWORD iCharSet,
	__in DWORD iOutPrecision,
	__in DWORD iClipPrecision,
	__in DWORD iQuality,
	__in DWORD iPitchAndFamily,
	__in_opt LPCSTR pszFaceName
);
#undef CreateFont
#define CreateFont CreateFontU

HFONT WINAPI CreateFontIndirectU(
	__in CONST LOGFONTA *lplf
);
#undef CreateFontIndirect
#define CreateFontIndirect CreateFontIndirectU

HFONT WINAPI CreateFontIndirectExU(
	__in CONST ENUMLOGFONTEXDVA *lpelfe
);
#undef CreateFontIndirectEx
#define CreateFontIndirectEx CreateFontIndirectExU

int WINAPI EnumFontFamiliesExU(
	__in HDC hdc,
	__in LPLOGFONTA lpLogfont,
	__in FONTENUMPROCA lpProc,
	__in LPARAM lParam,
	__in DWORD dwFlags
);
#undef EnumFontFamiliesEx
#define EnumFontFamiliesEx EnumFontFamiliesExU

BOOL APIENTRY GetTextExtentPoint32U(
	__in HDC hdc,
	__in_ecount(c) LPCSTR lpString,
	__in int c,
	__out LPSIZE psizl
);
#undef GetTextExtentPoint32
#define GetTextExtentPoint32 GetTextExtentPoint32U

BOOL WINAPI TextOutU(
	__in HDC hdc,
	__in int x,
	__in int y,
	__in_ecount(c) LPCSTR lpString,
	__in int c
);
#undef TextOut
#define TextOut TextOutU
