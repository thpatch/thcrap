/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Stuff related to text rendering.
  */

#include "thcrap.h"
#include "textdisp.h"

HFONT WINAPI textdisp_CreateFontA(
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
	int replaced = 0;
	const char *string_font;

	iCharSet = DEFAULT_CHARSET;

	// Check hardcoded strings and the run configuration for a replacement font.
	// Hardcoded strings take priority here.
	string_font = strings_lookup(pszFaceName, NULL);
	if(string_font != pszFaceName) {
		pszFaceName = string_font;
		replaced = 1;
	} else {
		json_t *run_font = json_object_get(run_cfg, "font");
		if(json_is_string(run_font)) {
			pszFaceName = json_string_value(run_font);
			replaced = 1;
		}
	}

	/**
	  * As long as we convert "ＭＳ ゴシック" to UTF-16, it will work on Western
	  * systems, too, provided that Japanese support is installed in the first place.
	  * No need to add an intransparent font substitution which annoys those
	  * who don't want to use translation patches at all.
	  */
	/*
	const wchar_t *japfonts = L"ＭＳ";
	// ＭＳ ゴシック
	if(!replaced && !wcsncmp(face_w, japfonts, 2)) {
		face_w = L"Calibri";
	}
	*/
	log_printf(
		"CreateFontA: %s%s %d (Weight %d, PitchAndFamily 0x%0x)\n",
		pszFaceName, replaced ? " (repl.)" : "", cHeight, cWeight, iPitchAndFamily
	);
	return CreateFont(
		cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic,
		bUnderline, bStrikeOut, iCharSet, iOutPrecision, iClipPrecision,
		iQuality, iPitchAndFamily, pszFaceName
	);
}

void patch_fonts_load(const json_t *patch_info)
{
	json_t *fonts = json_object_get(patch_info, "fonts");
	const char *font_fn;
	json_t *val;
	json_object_foreach(fonts, font_fn, val) {
		size_t font_size;
		void *font_buffer = patch_file_load(patch_info, font_fn, &font_size);

		if(font_buffer) {
			DWORD ret;
			log_printf("(Font) Loading %s (%d bytes)...\n", font_fn, font_size);
			AddFontMemResourceEx(font_buffer, font_size, NULL, &ret);
			SAFE_FREE(font_buffer);
			/**
			  * "However, when the process goes away, the system will unload the fonts
			  * even if the process did not call RemoveFontMemResource."
			  * http://msdn.microsoft.com/en-us/library/windows/desktop/dd183325%28v=vs.85%29.aspx
			  */
		}
	}
}

int textdisp_detour(HMODULE hMod)
{
	return iat_detour_funcs_var(hMod, "gdi32.dll", 1,
		"CreateFontA", textdisp_CreateFontA
	);
}

void textdisp_init()
{
	json_t *patches = json_object_get(runconfig_get(), "patches");
	size_t i;
	json_t *patch_info;
	json_array_foreach(patches, i, patch_info) {
		patch_fonts_load(patch_info);
	}
}
