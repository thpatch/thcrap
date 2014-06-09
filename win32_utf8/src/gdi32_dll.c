/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * gdi32.dll functions.
  */

#include "win32_utf8.h"

/// Font conversion helpers
/// -----------------------
static LOGFONTA* LogfontWToA(LOGFONTA *a, const LOGFONTW *w)
{
	if(w) {
		memcpy(a, w, offsetof(LOGFONT, lfFaceName));
		StringToUTF8(a->lfFaceName, w->lfFaceName, LF_FACESIZE);
	}
	return w ? a : NULL;
}

static LOGFONTW* LogfontAToW(LOGFONTW *w, const LOGFONTA *a)
{
	if(a) {
		memcpy(w, a, offsetof(LOGFONT, lfFaceName));
		StringToUTF16(w->lfFaceName, a->lfFaceName, LF_FACESIZE);
	}
	return a ? w : NULL;
}

static ENUMLOGFONTEXDVA* EnumLogfontExDVWToA(ENUMLOGFONTEXDVA *a, const ENUMLOGFONTEXDVW *w)
{
	if(w) {
		const ENUMLOGFONTEXW *elfe_w = &w->elfEnumLogfontEx;
		ENUMLOGFONTEXA *elfe_a = &a->elfEnumLogfontEx;
		// WinGDI.h says: "The actual size of the DESIGNVECTOR and
		// ENUMLOGFONTEXDV structures is determined by dvNumAxes,
		// MM_MAX_NUMAXES only detemines the maximal size allowed"
		DWORD dv_sizediff = (
			MM_MAX_NUMAXES - min(w->elfDesignVector.dvNumAxes, MM_MAX_NUMAXES)
		) * sizeof(LONG);

		LogfontWToA(&elfe_a->elfLogFont, &elfe_w->elfLogFont);
		StringToUTF8((char*)elfe_a->elfFullName, elfe_w->elfFullName, LF_FULLFACESIZE);
		StringToUTF8((char*)elfe_a->elfStyle, elfe_w->elfStyle, LF_FACESIZE);
		StringToUTF8((char*)elfe_a->elfScript, elfe_w->elfScript, LF_FACESIZE);
		memcpy(&a->elfDesignVector, &w->elfDesignVector, sizeof(DESIGNVECTOR) - dv_sizediff);
	}
	return w ? a : NULL;
}

static ENUMLOGFONTEXDVW* EnumLogfontExDVAToW(ENUMLOGFONTEXDVW *w, const ENUMLOGFONTEXDVA *a)
{
	if(w) {
		const ENUMLOGFONTEXA *elfe_a = &a->elfEnumLogfontEx;
		ENUMLOGFONTEXW *elfe_w = &w->elfEnumLogfontEx;
		DWORD dv_sizediff = (
			MM_MAX_NUMAXES - min(a->elfDesignVector.dvNumAxes, MM_MAX_NUMAXES)
		) * sizeof(LONG);

		LogfontAToW(&elfe_w->elfLogFont, &elfe_a->elfLogFont);
		StringToUTF16(elfe_w->elfFullName, (char*)elfe_a->elfFullName, LF_FULLFACESIZE);
		StringToUTF16(elfe_w->elfStyle, (char*)elfe_a->elfStyle, LF_FACESIZE);
		StringToUTF16(elfe_w->elfScript, (char*)elfe_a->elfScript, LF_FACESIZE);
		memcpy(&w->elfDesignVector, &a->elfDesignVector, sizeof(DESIGNVECTOR) - dv_sizediff);
	}
	return a ? w : NULL;
}

static ENUMTEXTMETRICA* EnumTextmetricWToA(ENUMTEXTMETRICA *a, const ENUMTEXTMETRICW *w)
{
	if(w) {
		const NEWTEXTMETRICW *ntm_w = &w->etmNewTextMetricEx.ntmTm;
		NEWTEXTMETRICA *ntm_a = &a->etmNewTextMetricEx.ntmTm;
		DWORD i = 0;

		memcpy(ntm_a, ntm_w, offsetof(NEWTEXTMETRIC, tmFirstChar));
		memcpy(&ntm_a->tmItalic, &ntm_w->tmItalic, sizeof(NEWTEXTMETRIC) - offsetof(NEWTEXTMETRIC, tmItalic));
		ntm_a->tmFirstChar = min(0xff, ntm_w->tmFirstChar);
		ntm_a->tmLastChar = min(0xff, ntm_w->tmLastChar);
		ntm_a->tmDefaultChar = min(0xff, ntm_w->tmDefaultChar);
		ntm_a->tmBreakChar = min(0xff, ntm_w->tmBreakChar);
		memcpy(&a->etmNewTextMetricEx.ntmFontSig, &w->etmNewTextMetricEx.ntmFontSig, sizeof(FONTSIGNATURE));
		memcpy(&a->etmAxesList, &w->etmAxesList, offsetof(AXESLIST, axlAxisInfo));
		for(i = 0; i < w->etmAxesList.axlNumAxes; i++) {
			const AXISINFOW *ai_w = &w->etmAxesList.axlAxisInfo[i];
			AXISINFOA *ai_a = &a->etmAxesList.axlAxisInfo[i];
			memcpy(ai_a, ai_w, offsetof(AXISINFO, axAxisName));
			StringToUTF8((char*)ai_a->axAxisName, ai_w->axAxisName, MM_MAX_AXES_NAMELEN);
		}
	}
	return w ? a : NULL;
}
/// -----------------------

/// Promotion wrappers
/// ------------------
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
)
{
	LOGFONTA lf_a = {
		cHeight, cWidth, cEscapement, cOrientation, cWeight, (BYTE)bItalic,
		(BYTE)bUnderline, (BYTE)bStrikeOut, (BYTE)iCharSet, (BYTE)iOutPrecision,
		(BYTE)iClipPrecision, (BYTE)iQuality, (BYTE)iPitchAndFamily
	};
	// Yes, Windows does the same internally. CreateFont() is *not* a way
	// to pass a face name longer than 32 characters.
	if(pszFaceName) {
		strncpy(lf_a.lfFaceName, pszFaceName, sizeof(lf_a.lfFaceName));
	}
	return down_func(&lf_a);
}

HFONT WINAPI lower_CreateFontIndirectA(
	__in CreateFontIndirectExA_type down_func,
	__in CONST LOGFONTA *lplf
)
{
	ENUMLOGFONTEXDVA elfedv_a;
	const size_t elfedv_lf_diff =
		sizeof(ENUMLOGFONTEXDVA) - offsetof(ENUMLOGFONTEXDVA, elfEnumLogfontEx.elfFullName)
	;
	if(!lplf) {
		return NULL;
	}
	memcpy(&elfedv_a.elfEnumLogfontEx.elfLogFont, lplf, sizeof(LOGFONTA));
	ZeroMemory(&elfedv_a.elfEnumLogfontEx.elfFullName, elfedv_lf_diff);
	return down_func(&elfedv_a);
}
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
)
{
	return lower_CreateFontA(CreateFontIndirectU,
		cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic,
		bUnderline, bStrikeOut, iCharSet, iOutPrecision,
		iClipPrecision, iQuality, iPitchAndFamily, pszFaceName
	);
}

HFONT WINAPI CreateFontIndirectU(
	__in CONST LOGFONTA *lplf
)
{
	return lower_CreateFontIndirectA(CreateFontIndirectExU, lplf);
}

HFONT WINAPI CreateFontIndirectExU(
	__in CONST ENUMLOGFONTEXDVA *lpelfe
)
{
	ENUMLOGFONTEXDVW elfedv_w;
	return CreateFontIndirectExW(EnumLogfontExDVAToW(&elfedv_w, lpelfe));
}

typedef struct {
	FONTENUMPROCA lpOrigProc;
	LPARAM lOrigParam;
} EnumFontFamExParam;

static int CALLBACK EnumFontFamExProcWrap(
	const ENUMLOGFONTEXDVW *lpelfe,
	const ENUMTEXTMETRICW *lpntme,
	DWORD FontType,
	EnumFontFamExParam *wrap_param
)
{
	if(wrap_param && wrap_param->lpOrigProc) {
		ENUMLOGFONTEXDVA elfedv_a;
		ENUMTEXTMETRICA etm_a;
		ENUMLOGFONTEXDVA *elfedv_a_ptr = EnumLogfontExDVWToA(&elfedv_a, lpelfe);
		ENUMTEXTMETRICA *etm_a_ptr = EnumTextmetricWToA(&etm_a, lpntme);
		return wrap_param->lpOrigProc(
			(LOGFONTA*)elfedv_a_ptr, (TEXTMETRICA*)etm_a_ptr, FontType, wrap_param->lOrigParam
		);
	}
	return 0;
}

int WINAPI EnumFontFamiliesExU(
	__in HDC hdc,
	__in LPLOGFONTA lpLogfont,
	__in FONTENUMPROCA lpProc,
	__in LPARAM lParam,
	__in DWORD dwFlags
)
{
	EnumFontFamExParam wrap_param = {lpProc, lParam};
	LOGFONTW lf_w;
	LOGFONTW *lf_w_ptr = LogfontAToW(&lf_w, lpLogfont);
	return EnumFontFamiliesExW(
		hdc, lf_w_ptr, (FONTENUMPROCW)EnumFontFamExProcWrap, (LPARAM)&wrap_param, dwFlags
	);
}

BOOL APIENTRY GetTextExtentPoint32U(
	__in HDC hdc,
	__in_ecount(c) LPCSTR lpString,
	__in int c,
	__out LPSIZE psizl
)
{
	BOOL ret;
	FixedLengthStringConvert(lpString, c);
	ret = GetTextExtentPoint32W(hdc, lpString_w, wcslen(lpString_w), psizl);
	WCHAR_T_FREE(lpString);
	return ret;
}

BOOL WINAPI TextOutU(
	__in HDC hdc,
	__in int x,
	__in int y,
	__in_ecount(c) LPCSTR lpString,
	__in int c
)
{
	BOOL ret;
	FixedLengthStringConvert(lpString, c);
	ret = TextOutW(hdc, x, y, lpString_w, wcslen(lpString_w));
	WCHAR_T_FREE(lpString);
	return ret;
}
