/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Win32 dialog patching.
  */

#include <thcrap.h>
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
  * This leaves us with 3 options:
  *
  *	1. Build accessible C structures from the resorce data, set translations,
  *	   get the target size, allocate the target buffer, and build (3 loops)
  *	2. Get the size of all original structures with translations applied,
  *	   allocate the target buffer, and build (2 loops)
  *	3. Just build the target buffer dynamically, reallocating space
  *	   on every step (1 loop)
  *
  * I went with option 2 here. 
  *
  * All that, however, doesn't help against the fact that Win32 dialog patching
  * is scary stuff. Every single line of code here has the potential to crash
  * the game or simply make the dialog not appear at all. So let's make the code
  * as as concise, abstracted, consistent and readable as possible:
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
typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
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

	// Only present if DS_SETFONT to be set in [style].
	DLGTEMPLATEEX_FONT font;
*/
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE DLGTEMPLATEEX_START;

// Font definition structure.
typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
	WORD pointsize;
	WORD weight;
	BYTE italic;
	BYTE charset;
/**
  * Variable parts of the structure that follow:
  *
	wchar_t typeface[];
*/
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE DLGTEMPLATEEX_FONT;

// Constant-width part of the dialog item structure
typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
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
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE DLGITEMTEMPLATEEX_START;

// Size adjustment state.
typedef struct {
	DLGTEMPLATEEX_START *dst_header;
	HDC hDC;
	HFONT hFont;
} dialog_adjust_t;
/// ----------

/// Helper functions
/// ----------------
size_t dword_align(const size_t val)
{
	return (val + 3) & ~3;
}

BYTE* ptr_dword_align(const BYTE *in)
{
	return (BYTE*)dword_align((UINT_PTR)in);
}

size_t ptr_advance(const unsigned char **src, size_t num)
{
	*src += num;
	return num;
}

size_t memcpy_advance_src(unsigned char *dst, const unsigned char **src, size_t num)
{
	memcpy(dst, *src, num);
	return ptr_advance(src, num);
}
/// ----------------

/// dialog_adjust_t
/// ---------------
void dialog_adjust_init(
	dialog_adjust_t *adj,
	DLGTEMPLATEEX_START *dst_header,
	const DLGTEMPLATEEX_FONT *font,
	const sz_Or_Ord *typeface
)
{
	if(!adj || !dst_header || !font || !typeface) {
		return;
	}
	ZeroMemory(adj, sizeof(*adj));
	adj->dst_header = dst_header;
	adj->hDC = GetDC(0);
	adj->hFont = CreateFontW(
		font->pointsize, 0, 0, 0, font->weight, font->italic, FALSE, FALSE,
		DEFAULT_CHARSET, 0, 0, PROOF_QUALITY, FF_DONTCARE, &typeface->sz
	);
	if(adj->hFont) {
		SelectObject(adj->hDC, adj->hFont);
	}
}

void dialog_adjust(
	dialog_adjust_t *adj,
	DLGITEMTEMPLATEEX_START *item,
	const char *str_new
)
{
	SIZE str_size;
	if(!adj || !adj->hDC || !item || !str_new) {
		return;
	}

	GetTextExtentPoint32(adj->hDC, str_new, strlen(str_new), &str_size);
	/**
	  * Why, Microsoft. Why.
	  * There seems to be *no way* to determine this programmatically.
	  * ... oh well, there are tons of dialog templates out there that implicitly
	  * depend on that value being roughly 12, so...
	  */
	if(
		item->style & BS_CHECKBOX ||
		item->style & BS_AUTOCHECKBOX ||
		item->style & BS_RADIOBUTTON ||
		item->style & BS_AUTORADIOBUTTON
	) {
		str_size.cx += 12;
	}
	if(str_size.cx > item->cx) {
		item->cx = str_size.cx;
	}
	if(item->cx > adj->dst_header->cx) {
		adj->dst_header->cx = item->cx;
	}
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
size_t sz_or_ord_size(const BYTE **src, const char *rep)
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
		if(rep) {
			ret = StringToUTF16(NULL, rep, -1) * sizeof(wchar_t);
		}
		return ret;
	} else {
		return 0;
	}
}

size_t sz_or_ord_build(BYTE *dst, const BYTE **src, const char *rep)
{
	if(dst && src) {
		const sz_Or_Ord *src_sz = (sz_Or_Ord*)*src;
		size_t dst_len = sz_or_ord_size(src, rep);

		if(rep) {
			StringToUTF16((wchar_t*)dst, rep, strlen(rep) + 1);
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
size_t dialog_template_ex_size(const BYTE **src, WORD *dlg_items, json_t *trans)
{
	if(src) {
		size_t ret = 0;
		const DLGTEMPLATEEX_START *dlg_struct = (DLGTEMPLATEEX_START*)*src;
		const char *trans_title = strings_get(json_object_get_string(trans, "title"));
		const char *trans_font = strings_get(json_object_get_string(trans, "font"));
	
		ret += ptr_advance(src, sizeof(DLGTEMPLATEEX_START));
		ret += sz_or_ord_size(src, NULL); // menu
		ret += sz_or_ord_size(src, NULL); // windowClass
		ret += sz_or_ord_size(src, trans_title); // title

		if(dlg_struct->style & DS_SETFONT) {
			ret += ptr_advance(src, sizeof(DLGTEMPLATEEX_FONT));
			ret += sz_or_ord_size(src, trans_font); // typeface
		}
		if(dlg_items) {
			*dlg_items = dlg_struct->cDlgItems;
		}
		*src = ptr_dword_align(*src);
		return dword_align(ret);
	} else {
		return 0;
	}
}

size_t dialog_template_ex_build(BYTE *dst, const BYTE **src, dialog_adjust_t *adj, json_t *trans)
{
	if(dst && src && *src) {
		BYTE *dst_start = dst;
		DLGTEMPLATEEX_START *dst_header = (DLGTEMPLATEEX_START*)dst_start;
		const char *trans_title = strings_get(json_object_get_string(trans, "title"));
		const char *trans_font = strings_get(json_object_get_string(trans, "font"));

		dst += memcpy_advance_src(dst, src, sizeof(DLGTEMPLATEEX_START));
		dst += sz_or_ord_build(dst, src, NULL); // menu
		dst += sz_or_ord_build(dst, src, NULL); // windowClass
		dst += sz_or_ord_build(dst, src, trans_title);

		if(dst_header->style & DS_SETFONT) {
			DLGTEMPLATEEX_FONT *font = (DLGTEMPLATEEX_FONT*)*src;
			size_t font_len;

			dst += memcpy_advance_src(dst, src, sizeof(DLGTEMPLATEEX_FONT));
			font_len = sz_or_ord_build(dst, src, trans_font); // typeface
			dialog_adjust_init(adj, dst_header, font, (sz_Or_Ord*)dst);
			dst += font_len;
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
size_t dialog_item_template_ex_size(const BYTE **src, json_t *trans)
{
	if(src) {
		size_t ret = 0;
		WORD extraCount;
		const char *trans_title = strings_get(json_string_value(trans));
	
		ret += ptr_advance(src, sizeof(DLGITEMTEMPLATEEX_START));
		ret += sz_or_ord_size(src, NULL); // windowClass
		ret += sz_or_ord_size(src, trans_title); // title
	
		extraCount = *(WORD*)*src;
		ret += ptr_advance(src, sizeof(WORD) + extraCount);

		*src = ptr_dword_align(*src);
		return dword_align(ret);
	} else {
		return 0;
	}
}

size_t dialog_item_template_ex_build(BYTE *dst, const BYTE **src, dialog_adjust_t *adj, json_t *trans)
{
	if(dst && src && *src) {
		BYTE *dst_start = dst;
		WORD extraCount;
		const char *trans_title = strings_get(json_string_value(trans));

		dst += memcpy_advance_src(dst, src, sizeof(DLGITEMTEMPLATEEX_START));
		dst += sz_or_ord_build(dst, src, NULL); // windowClass

		dialog_adjust(adj, (DLGITEMTEMPLATEEX_START*)dst_start, trans_title);

		dst += sz_or_ord_build(dst, src, trans_title);
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

	if(!lpTemplateName) {
		return NULL;
	}

	// MAKEINTRESOURCEA(5) == RT_DIALOG, which we can't use because <UNICODE>
	hrsrc = FindResourceA(hInstance, lpTemplateName, MAKEINTRESOURCEA(5));
	hDlg = LoadResource(hInstance, hrsrc);

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
		const char *dlg_suffix = ".js";
		size_t fn_len = strlen(dlg_prefix) + dlg_name_len + strlen(dlg_suffix) + 1;
		VLA(char, fn, fn_len);
		snprintf(fn, fn_len, dlg_format, dlg_prefix, lpTemplateName, dlg_suffix);
		trans = stack_game_json_resolve(fn, NULL);
	}
	if(trans) {
		const DLGTEMPLATEEX_START *dlg_in = (DLGTEMPLATEEX_START*)hDlg;
		json_t *trans_controls = json_object_get(trans, "items");

#ifdef _DEBUG
	Sleep(10000);
#endif
		if(dlg_in->dlgVer == 1 && dlg_in->signature == 0xffff) {
			const BYTE *src = (BYTE*)hDlg;
			BYTE *dst;
			dialog_adjust_t adj = {0};
			size_t i;
			WORD dlg_items;
			size_t dlg_out_len = 0;

			dlg_out_len += dialog_template_ex_size(&src, &dlg_items, trans);

			for(i = 0; i < dlg_items; i++) {
				json_t *trans_item = json_array_get(trans_controls, i);
				dlg_out_len += dialog_item_template_ex_size(&src, trans_item);
			}

			dlg_out = malloc(dlg_out_len);

			dst = (BYTE*)dlg_out;
			src = (BYTE*)hDlg; // reset

			dst += dialog_template_ex_build(dst, &src, &adj, trans);

			for(i = 0; i < dlg_items; i++) {
				json_t *trans_item = json_array_get(trans_controls, i);
				dst += dialog_item_template_ex_build(dst, &src, &adj, trans_item);
			};
			dialog_adjust_clear(&adj);
		}
	}
	json_decref(trans);
	return dlg_out;
}

HWND WINAPI dialog_CreateDialogParamA(
    __in_opt HINSTANCE hInstance,
    __in LPCSTR lpTemplateName,
    __in_opt HWND hWndParent,
    __in_opt DLGPROC lpDialogFunc,
    __in LPARAM dwInitParam
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
		ret = CreateDialogParamU(
			hInstance, lpTemplateName, hWndParent, lpDialogFunc, dwInitParam
		);
	}
	return ret;
}

INT_PTR WINAPI dialog_DialogBoxParamA(
    __in_opt HINSTANCE hInstance,
    __in LPCSTR lpTemplateName,
    __in_opt HWND hWndParent,
    __in_opt DLGPROC lpDialogFunc,
    __in LPARAM dwInitParam
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
		ret = DialogBoxParamU(
			hInstance, lpTemplateName, hWndParent, lpDialogFunc, dwInitParam
		);
	}
	return ret;
}

int dialog_patch(HMODULE hMod)
{
	return iat_patch_funcs_var(hMod, "user32.dll", 2,
		"CreateDialogParamA", dialog_CreateDialogParamA,
		"DialogBoxParamA", dialog_DialogBoxParamA
	);
}
