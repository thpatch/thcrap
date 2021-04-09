/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Win32 dialog patching.
  */

#include "thcrap.h"
#include "dialog.h"

/**
  * Oh boy. Where should I start.
  *
  * Win32 dialog resources consist of one DLGTEMPLATE(EX) followed by a number
  * of DLGITEMTEMPLATE(EX) structures. All structures are DWORD-aligned.
  * Now, these aren't actual C structures... because all their strings (which,
  * luckily, are always UTF-16) are *directly placed inside the structure*, in
  * all their variable-width glory.
  *
  * The only way to patch these is to rebuild all these structures completely,
  * replacing strings as needed.
  *
  * But wait, how much memory do we need for the new replacement dialog?
  *
  * This leaves us with 4 options:
  *
  *	1. Build accessible C structures from the resorce data, set translations,
  *	   get the target size, allocate the target buffer, and build (3 loops)
  *	2. Get the size of all original structures with translations applied,
  *	   allocate the target buffer, and build (2 loops)
  *	3. Just build the target buffer dynamically, reallocating space
  *	   on every step (1 loop)
  *	4. Like #2, but with a simplified size calculation - the new dialog can
  *	   never be bigger than the old dialog plus the size of all translated
  *	   strings in UTF-8. (1 loop, plus a trivial one)
  *
  * Option 4 clearly wins here.
  *
  * All that, however, doesn't help against the fact that Win32 dialog patching
  * is scary stuff. Every single line of code here has the potential to crash
  * the game or simply make the dialog not appear at all. So let's make the code
  * as concise, abstracted, consistent and readable as possible:
  *
  *	* There are three data types we care about: sz_Or_Ord (= everything that
  *	  can be a string), DLGTEMPLATEEX and DLGITEMTEMPLATEEX.
  *	* Each of these types has a _size function (to calculate the size of the
  *	  structure with applied translations) and a _build function (to actually
  *	  build that new translated structure).
  *
  *	* [src] is always advanced by helper functions.
  *	* Helper functions always return the patched length, which the callers
  *	  use to advance [dst].
  *
  *	All replacement strings are pulled from the string definition table.
  *	A dialog specification file ("dialog_<resourcename>.js") contains IDs of
  *	all translatable strings. The IDs for the DLGITEMTEMPLATEEX structures are
  *	stored in the array "items", in the order in which they appear in the
  *	original resource.
  */

/// Detour chains
/// -------------
W32U8_DETOUR_CHAIN_DEF(CreateDialogParam);
W32U8_DETOUR_CHAIN_DEF(DialogBoxParam);
/// -------------

/// Structures
/// ----------
// Ordinal-or-string structure
typedef union {
	struct {
		WORD ord_flag;
		// If ord_flag is 0x0000, this is not present,
		// and the structure is just 2 bytes long.
		WORD ord;
	};
	// Null-terminated string, stored in-place.
	wchar_t sz;
} sz_Or_Ord;

// Constant-width part of the dialog header structure
#pragma pack(push, 1)
typedef struct {
	WORD dlgVer;
	WORD signature;
	DWORD helpID;
	DWORD exStyle;
	DWORD style;
	WORD cDlgItems;
	short x;
	short y;
	short cx;
	short cy;
/**
  * Variable parts of the structure that follow:
  *
	// name or ordinal of a menu resource
	sz_Or_Ord menu;
	// name or ordinal of a window class
	sz_Or_Ord windowClass;
	// title string of the dialog box
	wchar_t title[];

	// Only present if DS_SETFONT is set in [style].
	DLGTEMPLATEEX_FONT font;
*/
} DLGTEMPLATEEX_START;
#pragma pack(pop)

// Font definition structure.
#pragma pack(push, 1)
typedef struct {
	WORD pointsize;
	WORD weight;
	BYTE italic;
	BYTE charset;
/**
  * Variable parts of the structure that follow:
  *
	wchar_t typeface[];
*/
} DLGTEMPLATEEX_FONT;
#pragma pack(pop)

// Constant-width part of the dialog item structure
#pragma pack(push, 1)
typedef struct {
	DWORD helpID;
	DWORD exStyle;
	DWORD style;
	short x;
	short y;
	short cx;
	short cy;
	DWORD id;
/**
  * Variable parts of the structure that follow:
  *
	sz_Or_Ord windowClass;
	sz_Or_Ord title;
	WORD extraCount;
	BYTE extraData[];
*/
} DLGITEMTEMPLATEEX_START;
#pragma pack(pop)

// Size adjustment state.
typedef struct {
	DLGTEMPLATEEX_START *dst_header;
	HDC hDC;
	HFONT hFont;
	LONG base_w;
	LONG base_h;
} dialog_adjust_t;
/// ----------

/// dialog_adjust_t
/// ---------------

/**
  * Wine source code below (dlls/gdi32/font.c).
  * This is an internal Windows function - it's present and even exported since
  * at least Windows XP (our minimum requirement anyway), but it's not supposed
  * to be public by any means, so we'll rather include a copy of it here.
  */

/***********************************************************************
 *           GdiGetCharDimensions    (GDI32.@)
 *
 * Gets the average width of the characters in the English alphabet.
 *
 * PARAMS
 *  hdc    [I] Handle to the device context to measure on.
 *  lptm   [O] Pointer to memory to store the text metrics into.
 *  height [O] On exit, the maximum height of characters in the English alphabet.
 *
 * RETURNS
 *  The average width of characters in the English alphabet.
 *
 * NOTES
 *  This function is used by the dialog manager to get the size of a dialog
 *  unit. It should also be used by other pieces of code that need to know
 *  the size of a dialog unit in logical units without having access to the
 *  window handle of the dialog.
 *  Windows caches the font metrics from this function, but we don't and
 *  there doesn't appear to be an immediate advantage to do so.
 *
 * SEE ALSO
 *  GetTextExtentPointW, GetTextMetricsW, MapDialogRect.
 */

LONG WINAPI GdiGetCharDimensions(HDC hdc, LPTEXTMETRICW lptm, LONG *height)
{
    SIZE sz;
    static const WCHAR alphabet[] = {
        'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q',
        'r','s','t','u','v','w','x','y','z','A','B','C','D','E','F','G','H',
        'I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',0};

    if(lptm && !GetTextMetricsW(hdc, lptm)) return 0;

    if(!GetTextExtentPointW(hdc, alphabet, 52, &sz)) return 0;

    if (height) *height = sz.cy;
    return (sz.cx / 26 + 1) / 2;
}

void dialog_adjust_init(
	dialog_adjust_t *adj,
	DLGTEMPLATEEX_START *dst_header,
	const DLGTEMPLATEEX_FONT *dst_font
)
{
	int font_height;
	const sz_Or_Ord *typeface = NULL;
	if(!adj || !dst_header || !dst_font) {
		return;
	}
	ZeroMemory(adj, sizeof(*adj));
	typeface = (sz_Or_Ord*)((BYTE*)dst_font + sizeof(DLGTEMPLATEEX_FONT));

	adj->dst_header = dst_header;
	adj->hDC = GetDC(0);
	font_height = -MulDiv(dst_font->pointsize, GetDeviceCaps(adj->hDC, LOGPIXELSY), 72);

	adj->hFont = CreateFontW(
		font_height, 0, 0, 0, dst_font->weight, dst_font->italic, FALSE, FALSE,
		DEFAULT_CHARSET, 0, 0, PROOF_QUALITY, FF_DONTCARE, &typeface->sz
	);
	/**
	  * We need to do MapDialogRect without a dialog window, so we need to derive
	  * the width and height of a dialog unit from the font.
	  * GetTextMetrics() does *not* give the right values, this seems to be the
	  * only function to work under all circumstances.
	  */
	if(adj->hFont) {
		SelectObject(adj->hDC, adj->hFont);
		adj->base_w = GdiGetCharDimensions(adj->hDC, NULL, &adj->base_h);
	}
}

void dialog_adjust(
	dialog_adjust_t *adj,
	DLGITEMTEMPLATEEX_START *item,
	const stringref_t rep
)
{
	RECT rect = {};
	UINT draw_flags;
	BYTE button_style;

	if(!adj || !adj->hDC || !item || !rep.data()) {
		return;
	}
	rect.right = item->cx;
	draw_flags = DT_CALCRECT | (strchr(rep.data(), '\n') ? DT_WORDBREAK : 0);
	DrawText(adj->hDC, rep.data(), rep.length(), &rect, draw_flags | DT_CALCRECT);

	// Convert pixels back to dialog units
	// (see http://msdn.microsoft.com/library/windows/desktop/ms645475%28v=vs.85%29.aspx)
	rect.right = MulDiv(rect.right, 4, adj->base_w);
	rect.bottom = MulDiv(rect.bottom, 8, adj->base_h);

	button_style = item->style & 0xff;
	/**
	  * Why, Microsoft. Why.
	  * There seems to be *no way* to determine this programmatically.
	  * ... oh well, there are tons of dialog templates out there that implicitly
	  * depend on that graphic being 13 + base_w pixels wide, so...
	  */
	if(
		(button_style == BS_CHECKBOX || button_style == BS_AUTOCHECKBOX ||
		 button_style == BS_RADIOBUTTON || button_style == BS_AUTORADIOBUTTON)
	) {
		rect.right += MulDiv(13 + adj->base_w, 4, adj->base_w);
	} else if(button_style == BS_PUSHBUTTON || button_style == BS_DEFPUSHBUTTON) {
		rect.right += 3;
	}
	item->cx = (short)MAX(rect.right, item->cx);
	item->cy = (short)MAX(rect.bottom, item->cy);
	adj->dst_header->cx = MAX(item->x + item->cx, adj->dst_header->cx);
	adj->dst_header->cy = MAX(item->y + item->cy, adj->dst_header->cy);
}

void dialog_adjust_clear(dialog_adjust_t *adj)
{
	if(!adj) {
		return;
	}
	DeleteObject(adj->hFont);
	ZeroMemory(adj, sizeof(*adj));
}
/// --------------

/// sz_or_Ord
/// ---------
size_t sz_or_ord_size(const BYTE **src)
{
	if(src) {
		const sz_Or_Ord *src_sz = (sz_Or_Ord*)*src;
		size_t ret;
		if(src_sz->ord_flag == 0) {
			ret = 2;
		} else if(src_sz->ord_flag == 0xffff) {
			ret = 4;
		} else {
			ret = (wcslen(&src_sz->sz) + 1) * sizeof(wchar_t);
		}
		*src += ret;
		return ret;
	} else {
		return 0;
	}
}

size_t sz_or_ord_build(BYTE *dst, const BYTE **src, const stringref_t *rep)
{
	if(dst && src) {
		const sz_Or_Ord *src_sz = (sz_Or_Ord*)*src;
		size_t dst_len = sz_or_ord_size(src);

		if(rep && rep->data()) {
			dst_len = StringToUTF16((wchar_t*)dst, rep->data(), rep->length() + 1) * sizeof(wchar_t);
		} else if(src_sz->ord_flag == 0 || src_sz->ord_flag == 0xffff) {
			memcpy(dst, src_sz, dst_len);
		} else {
			memcpy(dst, &src_sz->sz, dst_len);
		}
		return dst_len;
	} else {
		return 0;
	}
}
/// ---------

/// DLGTEMPLATEEX
/// -------------
size_t dialog_template_ex_size(json_t *trans)
{
	// Yup, everything error-checked already.
	size_t ret = 0;

	const json_t *trans_title = strings_get(json_object_get_string(trans, "title"));
	const json_t *trans_font = strings_get(json_object_get_string(trans, "font"));

	ret += json_string_length(trans_title) + 1;
	ret += json_string_length(trans_font) + 1;

	ret *= sizeof(wchar_t);
	return dword_align(ret);
}

size_t dialog_template_ex_build(BYTE *dst, const BYTE **src, dialog_adjust_t *adj, json_t *trans)
{
	if(dst && src && *src) {
		BYTE *dst_start = dst;
		DLGTEMPLATEEX_START *dst_header = (DLGTEMPLATEEX_START*)dst_start;
		stringref_t trans_title = strings_get(json_object_get_string(trans, "title"));
		stringref_t trans_font = strings_get(json_object_get_string(trans, "font"));
		WORD trans_font_size = (WORD)json_object_get_hex(trans, "font_size");

		dst += memcpy_advance_src(dst, src, sizeof(DLGTEMPLATEEX_START));
		dst += sz_or_ord_build(dst, src, NULL); // menu
		dst += sz_or_ord_build(dst, src, NULL); // windowClass
		dst += sz_or_ord_build(dst, src, &trans_title);

		if(dst_header->style & DS_SETFONT) {
			DLGTEMPLATEEX_FONT *dst_font = (DLGTEMPLATEEX_FONT*)dst;
			size_t len;

			len = memcpy_advance_src(dst, src, sizeof(DLGTEMPLATEEX_FONT));
			if(trans_font_size) {
				((DLGTEMPLATEEX_FONT*)dst)->pointsize = trans_font_size;
			}
			dst += len;

			/*
			 * From Windows 2000 on, this is done behind the scenes inside
			 * DialogBox() / CreateDialog(), but *not* in CreateFont().
			 * So we need to do it ourselves to get the correct text lengths.
			 */
			auto src_font = reinterpret_cast<const wchar_t*>(*src);
			if(
				(trans_font.data() && !stricmp(trans_font.data(), "MS Shell Dlg"))
				|| (src_font && !wcsicmp(src_font, L"MS Shell Dlg"))
			) {
				trans_font = "MS Shell Dlg 2";
			}

			dst += sz_or_ord_build(dst, src, &trans_font); // typeface
			dialog_adjust_init(adj, dst_header, dst_font);
		}
		*src = ptr_dword_align(*src);
		return ptr_dword_align(dst) - dst_start;
	} else {
		return 0;
	}
}
/// -------------

/// DLGITEMTEMPLATEEX
/// -----------------
size_t dialog_item_template_ex_size(json_t *trans)
{
	// Yup, everything error-checked already.
	size_t ret = 0;

	const json_t *trans_title = strings_get(json_string_value(trans));

	ret += json_string_length(trans_title) + 1;

	ret *= sizeof(wchar_t);
	return dword_align(ret);
}

size_t dialog_item_template_ex_build(BYTE *dst, const BYTE **src, dialog_adjust_t *adj, json_t *trans)
{
	if(dst && src && *src) {
		BYTE *dst_start = dst;
		WORD extraCount;
		stringref_t trans_title = strings_get(json_string_value(trans));

		dst += memcpy_advance_src(dst, src, sizeof(DLGITEMTEMPLATEEX_START));
		dst += sz_or_ord_build(dst, src, NULL); // windowClass

		dialog_adjust(adj, (DLGITEMTEMPLATEEX_START*)dst_start, trans_title);

		dst += sz_or_ord_build(dst, src, &trans_title);
		extraCount = *((WORD*)*src);
		dst += memcpy_advance_src(dst, src, extraCount + sizeof(WORD));

		*src = ptr_dword_align(*src);
		return ptr_dword_align(dst) - dst_start;
	} else {
		return 0;
	}
}
/// -----------------

DLGTEMPLATE* dialog_translate(HINSTANCE hInstance, LPCSTR lpTemplateName)
{
	void *dlg_out = NULL;
	json_t *trans = NULL;
	size_t dlg_name_len;
	const char *dlg_format = NULL;
	HRSRC hrsrc;
	HGLOBAL hDlg;
	HGLOBAL hDlg_rep = NULL;
	size_t hDlg_len;

	if(!lpTemplateName) {
		return NULL;
	}

	// MAKEINTRESOURCE(5) == RT_DIALOG. (The regular
	// RT_DIALOG macro is subject to UNICODE.)
	hrsrc = FindResourceA(hInstance, lpTemplateName, MAKEINTRESOURCEA(5));
	hDlg = LoadResource(hInstance, hrsrc);
	hDlg_len = SizeofResource(hInstance, hrsrc);

	if(!hDlg) {
		return NULL;
	}

	if(HIWORD(lpTemplateName) != 0) {
		dlg_name_len = strlen(lpTemplateName) + 1;
		dlg_format = "%s%s%s";
	} else  {
		dlg_name_len = str_num_digits(LOWORD(lpTemplateName)) + 1;
		dlg_format = "%s%u%s";
	}
	{
		const char *dlg_prefix = "dialog_";
		const char *dlg_suffix_bin = ".bin";
		const char *dlg_suffix_js = ".js";
		size_t fn_len = strlen(dlg_prefix) + dlg_name_len + strlen(dlg_suffix_bin) + 1;
		VLA(char, fn, fn_len);
		size_t hDlg_rep_len;

		snprintf(fn, fn_len, dlg_format, dlg_prefix, lpTemplateName, dlg_suffix_bin);
		hDlg_rep = stack_game_file_resolve(fn, &hDlg_rep_len);

		if(hDlg_rep) {
			hDlg = hDlg_rep;
			hDlg_len = hDlg_rep_len;
		}

		PathRenameExtensionA(fn, dlg_suffix_js);
		trans = stack_game_json_resolve(fn, NULL);
		VLA_FREE(fn);
	}
	if(trans) {
		const DLGTEMPLATEEX_START *dlg_in = (DLGTEMPLATEEX_START*)hDlg;
		json_t *trans_controls = json_object_get(trans, "items");

		if(dlg_in->dlgVer == 1 && dlg_in->signature == 0xffff) {
			const BYTE *src = (BYTE*)hDlg;
			BYTE *dst;
			dialog_adjust_t adj = {};
			size_t i;
			size_t dlg_out_len = hDlg_len;

			dlg_out_len += dialog_template_ex_size(trans);

			for(i = 0; i < dlg_in->cDlgItems; i++) {
				json_t *trans_item = json_array_get(trans_controls, i);
				dlg_out_len += dialog_item_template_ex_size(trans_item);
			}

			dlg_out = malloc(dlg_out_len);

			dst = (BYTE*)dlg_out;
			src = (BYTE*)hDlg; // reset

			dst += dialog_template_ex_build(dst, &src, &adj, trans);

			for(i = 0; i < dlg_in->cDlgItems; i++) {
				json_t *trans_item = json_array_get(trans_controls, i);
				dst += dialog_item_template_ex_build(dst, &src, &adj, trans_item);
			};
			dialog_adjust_clear(&adj);
		}
	}
	json_decref(trans);
	SAFE_FREE(hDlg_rep);
	return (DLGTEMPLATE *)dlg_out;
}

HWND WINAPI dialog_CreateDialogParamA(
	HINSTANCE hInstance,
	LPCSTR lpTemplateName,
	HWND hWndParent,
	DLGPROC lpDialogFunc,
	LPARAM dwInitParam
)
{
	HWND ret;
	DLGTEMPLATE *dlg_trans = dialog_translate(hInstance, lpTemplateName);
	if(dlg_trans) {
		ret = CreateDialogIndirectParamW(
			hInstance, dlg_trans, hWndParent, lpDialogFunc, dwInitParam
		);
		SAFE_FREE(dlg_trans);
	} else {
		ret = chain_CreateDialogParamU(
			hInstance, lpTemplateName, hWndParent, lpDialogFunc, dwInitParam
		);
	}
	return ret;
}

INT_PTR WINAPI dialog_DialogBoxParamA(
	HINSTANCE hInstance,
	LPCSTR lpTemplateName,
	HWND hWndParent,
	DLGPROC lpDialogFunc,
	LPARAM dwInitParam
)
{
	INT_PTR ret;
	DLGTEMPLATE *dlg_trans = dialog_translate(hInstance, lpTemplateName);
	if(dlg_trans) {
		ret = DialogBoxIndirectParamW(
			hInstance, dlg_trans, hWndParent, lpDialogFunc, dwInitParam
		);
		SAFE_FREE(dlg_trans);
	} else {
		ret = chain_DialogBoxParamU(
			hInstance, lpTemplateName, hWndParent, lpDialogFunc, dwInitParam
		);
	}
	return ret;
}

void dialog_mod_detour(void)
{
	detour_chain("user32.dll", 1,
		"CreateDialogParamA", dialog_CreateDialogParamA, &chain_CreateDialogParamU,
		"DialogBoxParamA", dialog_DialogBoxParamA, &chain_DialogBoxParamU,
		NULL
	);
}
