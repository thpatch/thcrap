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
	  * CreateFont() prioritizes [iCharSet] and ensures that the final font
	  * can display the given charset. If the font given in [pszFaceName]
	  * doesn't claim to cover the range of codepoints used in [iCharSet],
	  * Windows will just ignore [pszFaceName], select a different font
	  * that does, and return that instead.
	  * To ensure that we actually get the named font, we instead specify
	  * DEFAULT_CHARSET, which refers to the charset of the current system
	  * locale. Yes, there's unfortunately no DONTCARE_CHARSET, but this
	  * should be fine for most use cases.
	  *
	  * However, we only do this if we *have* a face name, either from the
	  * original code or the run configuration. If [pszFaceName] is NULL or
	  * empty, CreateFont() should simply use the default system font for
	  * [iCharSet], and DEFAULT_CHARSET might result in a different font.
	  */
	if(pszFaceName && pszFaceName[0]) {
		iCharSet = DEFAULT_CHARSET;
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
		"CreateFontA: %s%s %d (Weight %d, CharSet %d, PitchAndFamily 0x%0x)\n",
		pszFaceName, replaced ? " (repl.)" : "",
		cHeight, cWeight, iCharSet, iPitchAndFamily
	);
	return (HFONT)detour_next(
		"gdi32.dll", "CreateFontA", textdisp_CreateFontA, 14,
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

void textdisp_mod_detour(void)
{
	detour_cache_add("gdi32.dll",
		"CreateFontA", textdisp_CreateFontA,
		NULL
	);
}

void textdisp_mod_init(void)
{
	json_t *patches = json_object_get(runconfig_get(), "patches");
	size_t i;
	json_t *patch_info;
	json_array_foreach(patches, i, patch_info) {
		patch_fonts_load(patch_info);
	}
}
