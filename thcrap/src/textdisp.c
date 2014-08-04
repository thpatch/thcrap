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

/// Font rules
/// ----------
// Fills [arg] with the font rule argument at [str].
// Returns [str], advanced to the next argument.
const char* fontrule_arg(char *arg, size_t arg_len, const char *str)
{
	size_t i = 0;
	char delim = ' ';
	if(!arg || !arg_len || !str) {
		return str;
	}
	if(*str == '\'') {
		delim = '\'';
		str++;
	}
	while(i < (arg_len - 1) && *str && *str != delim) {
		arg[i++] = *str++;
	}
	arg[i] = '\0';
	// Skip any garbage after the argument...
	while(*str && *str != ' ') {
		str++;
	}
	// ...and skip from there to the next one.
	while(*str && *str == ' ') {
		str++;
	}
	return str;
}

int fontrule_arg_int(const char **str, int *score)
{
	int ret;
	char arg[LF_FACESIZE] = {0};
	if(!str || !*str) {
		return 0;
	}
	*str = fontrule_arg(arg, sizeof(arg), *str);
	ret = atoi(arg);
	if(score) {
		*score += ret != 0;
	}
	return ret;
}

// Returns the amount of LOGFONT parameters (the "score") contained in [str].
int fontrule_parse(LOGFONTA *lf, const char *str)
{
	int score = 0;
	if(!lf || !str) {
		return -1;
	}
	if(*str == '\'') {
		str = fontrule_arg(lf->lfFaceName, LF_FACESIZE, str);
		score++;
	} else {
		lf->lfPitchAndFamily = fontrule_arg_int(&str, &score);
		lf->lfFaceName[0] = '\0';
	}
	lf->lfHeight = fontrule_arg_int(&str, &score);
	lf->lfWidth = fontrule_arg_int(&str, &score);
	lf->lfWeight = fontrule_arg_int(&str, &score);
	return score;
}

#define FONTRULE_MATCH(rule, dst, val) \
	if(rule##->##val && rule##->##val != dst##->##val) {\
		return 0; \
	}

#define FONTRULE_APPLY(dst, rep, val) \
	if(rep##->##val && priority ? 1 : !dst##->##val) { \
		dst##->##val = rep##->##val; \
	}

#define FONTRULE_MACRO_EXPAND(macro, p1, p2) \
	macro(p1, p2, lfPitchAndFamily); \
	macro(p1, p2, lfHeight); \
	macro(p1, p2, lfWidth); \
	macro(p1, p2, lfWeight);

// Returns 1 if the relevant nonzero members in [rule] match those in [dst].
int fontrule_match(const LOGFONTA *rule, const LOGFONTA *dst)
{
	if(rule->lfFaceName[0] && strncmp(
		rule->lfFaceName, dst->lfFaceName, LF_FACESIZE
	)) {
		return 0;
	}
	FONTRULE_MACRO_EXPAND(FONTRULE_MATCH, rule, dst);
	return 1;
}

// Copies all relevant nonzero members from [rep] to [dst]. If [priority] is
// zero, values are only copied if the same value is not yet set in [dst].
int fontrule_apply(LOGFONTA *dst, const LOGFONTA *rep, int priority)
{
	if(rep->lfFaceName[0] && priority ? 1 : !dst->lfFaceName[0]) {
		strncpy(dst->lfFaceName, rep->lfFaceName, LF_FACESIZE);
	}
	FONTRULE_MACRO_EXPAND(FONTRULE_APPLY, dst, rep);
	return 1;
}

// Returns 1 if a font rule was applied, 0 otherwise.
int fontrules_apply(LOGFONTA *lf)
{
	json_t *fontrules = json_object_get(runconfig_get(), "fontrules");
	LOGFONTA rep_full = {0};
	int rep_score = 0;
	int log_header = 0;
	const char *key;
	json_t *val;
	if(!lf) {
		return -1;
	}
	json_object_foreach(fontrules, key, val) {
		LOGFONTA rule = {0};
		int rule_score = fontrule_parse(&rule, key);
		int priority = rule_score >= rep_score;
		const char *rep_str = json_string_value(val);
		if(!fontrule_match(&rule, lf)) {
			continue;
		}
		if(!log_header) {
			log_printf(
				"(Font) Replacement rules applied to %s:\n",
				logfont_stringify(lf)
			);
			log_header = 1;
		}
		fontrule_parse(&rule, rep_str);
		log_printf(
			"(Font) • (%s) → (%s)%s\n",
			key, rep_str, priority ? " (priority)" : ""
		);
		fontrule_apply(&rep_full, &rule, priority);
		rep_score = max(rule_score, rep_score);
	}
	return fontrule_apply(lf, &rep_full, 1);
}
/// ------------------

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
	fontrules_apply(lf);
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
			log_printf("(Police) Chargement de %s (%d octets)...\n", font_fn, font_size);
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
