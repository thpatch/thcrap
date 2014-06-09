/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Win32 detour function level promotion.
  */

#include "thcrap.h"
#include "promote.h"

/// CreateFont() family (promoting to CreateFontIndirectExA)
/// -------------------
HFONT WINAPI promote_CreateFontA(
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
)
{
	CreateFontIndirectA_type target = (CreateFontIndirectA_type)detour_top(
		"gdi32.dll", "CreateFontIndirectA", (FARPROC)CreateFontIndirectU
	);
	return lower_CreateFontA(target,
		cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic,
		bUnderline, bStrikeOut, iCharSet, iOutPrecision,
		iClipPrecision, iQuality, iPitchAndFamily, pszFaceName
	);
}

HFONT WINAPI promote_CreateFontIndirectA(
	__in CONST LOGFONTA *lplf
)
{
	CreateFontIndirectExA_type target = (CreateFontIndirectExA_type)detour_top(
		"gdi32.dll", "CreateFontIndirectExA", (FARPROC)CreateFontIndirectExU
	);
	return lower_CreateFontIndirectA(target, lplf);
}
/// -------------------

void promote_mod_init(void)
{
	detour_chain("gdi32.dll", 0,
		"CreateFontIndirectA", promote_CreateFontIndirectA,
		"CreateFontA", promote_CreateFontA,
		NULL
	);
}
