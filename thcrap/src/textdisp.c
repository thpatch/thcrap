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

/// Detour chains
/// -------------
DETOUR_CHAIN_DEF(CreateFontU);
// Type-safety is nice
static CreateFontIndirectExA_type chain_CreateFontIndirectExU = CreateFontIndirectExU;
/// -------------

/// LOGFONT stringification
/// -----------------------
const char* logfont_stringify(const LOGFONTA *lf)
{
	char face[LF_FACESIZE + 2];
	if(!lf) {
		return NULL;
	}
	if(lf->lfFaceName[0]) {
		snprintf(face, sizeof(face), "'%s'", lf->lfFaceName);
	} else {
		snprintf(face, sizeof(face), "%u", lf->lfPitchAndFamily);
	}
	return strings_sprintf((size_t)lf, "(%s %d %d %d)",
		face, lf->lfHeight, lf->lfWidth, lf->lfWeight
	);
}
/// -----------------------

// This detour is kept for backwards compatibility to patch configurations
// that replace multiple fonts via hardcoded string translation. Due to the
// fact that lower levels copy [pszFaceName] into the LOGFONT structure,
// it would be impossible to look up a replacement font name there.
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
	// Check hardcoded strings and the run configuration for a replacement
	// font. Hardcoded strings take priority here.
	const char *string_font = strings_lookup(pszFaceName, NULL);
	if(string_font != pszFaceName) {
		pszFaceName = string_font;
	} else {
		json_t *run_font = json_object_get(run_cfg, "font");
		if(json_is_string(run_font)) {
			pszFaceName = json_string_value(run_font);
		}
	}
	return (HFONT)chain_CreateFontU(
		cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic,
		bUnderline, bStrikeOut, iCharSet, iOutPrecision, iClipPrecision,
		iQuality, iPitchAndFamily, pszFaceName
	);
}

HFONT WINAPI textdisp_CreateFontIndirectExA(
	__in ENUMLOGFONTEXDVA *lpelfe
)
{
	LOGFONTA *lf = NULL;
	wchar_t face_w[LF_FACESIZE];
	if(!lpelfe) {
		return NULL;
	}
	lf = &lpelfe->elfEnumLogfontEx.elfLogFont;
	// Ensure that the font face is in UTF-8
	StringToUTF16(face_w, lf->lfFaceName, LF_FACESIZE);
	StringToUTF8(lf->lfFaceName, face_w, LF_FACESIZE);
	/**
	  * CreateFont() prioritizes [lfCharSet] and ensures that the font
	  * created can display the given charset. If the font given in
	  * [lfFaceName] is not found *or* does not claim to cover the range of
	  * codepoints used in [lfCharSet], GDI will just ignore [lfFaceName]
	  * and instead select the font that provides the closest match for
	  * [lfPitchAndFamily], out of all installed fonts that cover
	  * [lfCharSet].
	  *
	  * To ensure that we actually get the named font, we instead specify
	  * DEFAULT_CHARSET, which refers to the charset of the current system
	  * locale. Yes, there's unfortunately no DONTCARE_CHARSET, but this
	  * should be fine for most use cases.
	  *
	  * However, we only do this if we *have* a face name, either from the
	  * original code or the run configuration. If [lfFaceName] is NULL or
	  * empty, the original intention of the CreateFont() call was indeed
	  * to select the default match for [lfPitchAndFamily] and [lfCharSet].
	  * DEFAULT_CHARSET might therefore result in a different font.
	  */
	if(lf->lfFaceName[0]) {
		lf->lfCharSet = DEFAULT_CHARSET;
	}
	log_printf("(Font) Creating %s...\n", logfont_stringify(lf));
	return chain_CreateFontIndirectExU(lpelfe);
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
	detour_chain("gdi32.dll", 1,
		"CreateFontA", textdisp_CreateFontA, &chain_CreateFontU,
		"CreateFontIndirectExA", textdisp_CreateFontIndirectExA, &chain_CreateFontIndirectExU,
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
