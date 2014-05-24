/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * gdi32.dll functions.
  */

#include "win32_utf8.h"

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
	HFONT ret;
	WCHAR_T_DEC(pszFaceName);
	WCHAR_T_CONV_VLA(pszFaceName);
	ret = CreateFontW(
		cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic,
		bUnderline, bStrikeOut, iCharSet, iOutPrecision, iClipPrecision,
		iQuality, iPitchAndFamily, pszFaceName_w
	);
	WCHAR_T_FREE(pszFaceName);
	return ret;
}

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
