/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Functions about fonts and character sets.
  */

#include "thcrap.h"

// List from https://msdn.microsoft.com/en-us/library/cc194829.aspx
static const struct {
	BYTE charset;
	WORD cp;
} charset_to_codepage[] = {
	{SHIFTJIS_CHARSET,    932},
	{HANGUL_CHARSET,      949},
	{GB2312_CHARSET,      936},
	{CHINESEBIG5_CHARSET, 950},
	{GREEK_CHARSET,       1253},
	{TURKISH_CHARSET,     1254},
	{HEBREW_CHARSET,      1255},
	{ARABIC_CHARSET,      1256},
	{BALTIC_CHARSET,      1257},
	{RUSSIAN_CHARSET,     1251},
	{THAI_CHARSET,        874},
	{ANSI_CHARSET,        1252},
	{UCHAR_MAX,           0}
};

int font_has_character(HDC hdc, WCHAR c)
{
	DWORD size = GetFontUnicodeRanges(hdc, nullptr);
	LPGLYPHSET glyphset = (LPGLYPHSET)malloc(size);
	GetFontUnicodeRanges(hdc, glyphset);
	for (DWORD i = 0; i < glyphset->cRanges; i++) {
		if (c >= glyphset->ranges[i].wcLow && c < glyphset->ranges[i].wcLow + glyphset->ranges[i].cGlyphs) {
			free(glyphset);
			return 1;
		}
	}
	free(glyphset);
	return 0;
}

BYTE character_to_charset(WCHAR c)
{
	const char *defaultChar = "?";
	char dst[5];

	for (int i = 0; charset_to_codepage[i].cp != 0; i++) {
		BOOL usedDefaultChar = FALSE;
		if (WideCharToMultiByte(charset_to_codepage[i].cp, 0, &c, 1, dst, 5, defaultChar, &usedDefaultChar) > 0 && usedDefaultChar == FALSE) {
			return charset_to_codepage[i].charset;
		}
	}
	return UCHAR_MAX;
}

HFONT font_create_for_character(const LOGFONTW *lplf, WORD c)
{
	BYTE charset = character_to_charset(c);
	if (charset == UCHAR_MAX) {
		return NULL;
	}

	LOGFONTW lf;
	memcpy(&lf, lplf, sizeof(*lplf));
	lf.lfCharSet = charset;
	memset(lf.lfFaceName, 0, sizeof(lf.lfFaceName));
	return CreateFontIndirectW(&lf);
}
