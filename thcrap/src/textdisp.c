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

/// Functions
/// ---------
// Types
typedef HDC (WINAPI *DLL_FUNC_TYPE(gdi32, CreateCompatibleDC))(HDC);
typedef BOOL (WINAPI *DLL_FUNC_TYPE(gdi32, DeleteDC))(HDC);
typedef HGDIOBJ (WINAPI *DLL_FUNC_TYPE (gdi32, SelectObject))(HDC, HGDIOBJ);

// Pointers
DLL_FUNC_DEF(gdi32, CreateCompatibleDC);
DLL_FUNC_DEF(gdi32, DeleteDC);
DLL_FUNC_DEF(gdi32, SelectObject);
/// ---------

// Device context
static HDC text_dc = NULL;

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
	json_t *run_font;
	
	iCharSet = DEFAULT_CHARSET;

	// Read external font
	run_font = json_object_get(run_cfg, "font");
	if(json_is_string(run_font)) {
		pszFaceName = json_string_value(run_font);
		replaced = 1;
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

HGDIOBJ WINAPI textdisp_SelectObject(
	__in HDC hdc,
	__in HGDIOBJ h
	)
{
	if(h == GetStockObject(SYSTEM_FONT)) {
		return GetCurrentObject(hdc, OBJ_FONT);
	} else {
		return DLL_FUNC(gdi32, SelectObject)(hdc, h);
	}
}

// Games prior to MoF call this every time a text is rendered.
// However, we require the DC to be present at all times for GetTextExtent.
// Thus, we keep only one DC in memory (the game doesn't need more anyway).
HDC WINAPI textdisp_CreateCompatibleDC( __in_opt HDC hdc)
{
	if(!text_dc) {
		HDC ret = DLL_FUNC(gdi32, CreateCompatibleDC)(hdc);
		text_dc = ret;
		log_printf("CreateCompatibleDC(0x%8x) -> 0x%8x\n", hdc, ret);
	}
	return text_dc;
}

BOOL WINAPI textdisp_DeleteDC( __in HDC hdc)
{
	// Bypass this function - we delete our DC on textdisp_exit()
	return 1;
}

size_t __stdcall GetTextExtent(const char *str)
{
	SIZE size;
	size_t str_len = strlen(str) + 1;

	ZeroMemory(&size, sizeof(SIZE));

	if(!text_dc) {
		// Do... something
	}

	GetTextExtentPoint32(text_dc, str, str_len, &size);
	log_printf("GetTextExtent('%s') = %d -> %d\n", str, size.cx, size.cx / 2);
	return (size.cx / 2);
}

size_t __stdcall GetTextExtentForFont(const char *str, HGDIOBJ font)
{
	HGDIOBJ prev_font = textdisp_SelectObject(text_dc, font);
	size_t ret = GetTextExtent(str);
	textdisp_SelectObject(text_dc, prev_font);
	return ret;
}

void patch_fonts_load(const json_t *patch_info, const json_t *patch_js)
{
	json_t *fonts = json_object_get(patch_js, "fonts");
	const char *font_fn;
	json_t *val;
	json_object_foreach(fonts, font_fn, val) {
		void *font_buffer = NULL;
		size_t font_size;

		font_buffer = patch_file_load(patch_info, font_fn, &font_size);

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

int textdisp_init(HMODULE hMod)
{
	HMODULE hGDI32 = GetModuleHandleA("gdi32.dll");
	if(!hGDI32) {
		return -1;
	}
	// Required functions
	DLL_GET_PROC_ADDRESS(hGDI32, gdi32, CreateCompatibleDC);
	DLL_GET_PROC_ADDRESS(hGDI32, gdi32, DeleteDC);
	DLL_GET_PROC_ADDRESS(hGDI32, gdi32, SelectObject);
	return 0;
}

int textdisp_patch(HMODULE hMod)
{
	return iat_patch_funcs_var(hMod, "gdi32.dll", 4,
		"CreateFontA", textdisp_CreateFontA,
		"CreateCompatibleDC", textdisp_CreateCompatibleDC,
		"DeleteDC", textdisp_DeleteDC,
		"SelectObject", textdisp_SelectObject
	);
}

void textdisp_exit()
{
	if(DLL_FUNC(gdi32, DeleteDC) && text_dc) {
		DLL_FUNC(gdi32, DeleteDC)(text_dc);
	}
}
