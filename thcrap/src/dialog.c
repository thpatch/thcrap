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

// * DLGTEMPLATE(EX) are not real structures
// * These are models with correct pointers, blah

typedef union {
	struct {
		WORD ord_flag;
		// If ord_flag is 0x0000, this isn't even there
		WORD ord;
	};
	LPCWSTR sz;
} sz_Or_Ord;

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
	// name or ordinal of a menu resource
	sz_Or_Ord menu;
	// name or ordinal of a window class
	sz_Or_Ord windowClass;
	// title string of the dialog box
	LPCWSTR title;

	// The following need DS_SETFONT to be set in [style].
	WORD pointsize;
	WORD weight;
	BYTE italic;
	BYTE charset;
	LPCWSTR typeface;
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE DLGTEMPLATEEX_MOD;

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
	sz_Or_Ord windowClass;
	sz_Or_Ord title;
	WORD extraCount;
	const void *extraData;
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE DLGITEMTEMPLATEEX_MOD;

unsigned char* memcpy_advance_src(void *dst, const void *src, size_t num)
{
	memcpy(dst, src, num);
	return (unsigned char*)src + num;
}

unsigned char* memcpy_advance_dst(void *dst, const void *src, size_t num)
{
	memcpy(dst, src, num);
	return (unsigned char*)dst + num;
}

const BYTE* wcs_assign_advance_src(const wchar_t **dst, const wchar_t *src)
{
	*dst = src;
	// Pointer arithmetic!
	return (const BYTE*)src + (wcslen(*dst) + 1) * 2;
}

// Returns [src] plus its actual size.
const BYTE* sz_or_ord_parse(sz_Or_Ord* dst, const sz_Or_Ord* src)
{
	if(!dst || !src) {
		return NULL;
	}
	if(src->ord_flag == 0) {
		dst->ord_flag = 0;
		dst->ord = 0;
		return (BYTE*)(src) + sizeof(WORD);
	} else if(src->ord_flag == 0xffff) {
		return memcpy_advance_src(dst, src, sizeof(sz_Or_Ord));
	} else {
		return wcs_assign_advance_src(&dst->sz, (LPCWSTR)&src->sz);
	}
}

size_t sz_or_ord_rep(const sz_Or_Ord* obj, const char *rep)
{
	if(!obj) {
		return 0;
	} else if(rep) {
		return MultiByteToWideChar(CP_UTF8, 0, rep, -1, NULL, 0) * sizeof(wchar_t);
	} else if(obj->ord_flag == 0) {
		return 2;
	} else if(obj->ord_flag == 0xffff) {
		return 4;
	} else {
		return (wcslen(obj->sz) + 1) * sizeof(wchar_t);
	}
}

BYTE* sz_or_ord_build(BYTE* dst, const sz_Or_Ord* src, const char *rep)
{
	size_t dst_len = sz_or_ord_rep(src, rep);
	if(!dst || !src) {
		return NULL;
	}
	if(rep) {
		StringToUTF16((wchar_t*)dst, rep, strlen(rep) + 1);
	} else if(src->ord_flag == 0 || src->ord_flag == 0xffff) {
		memcpy(dst, src, dst_len);
	} else {
		memcpy(dst, src->sz, dst_len);
	}
	return dst + dst_len;
}

// Returns [src] plus its actual size.
const BYTE* dialog_template_ex_parse(DLGTEMPLATEEX_MOD *dst, const BYTE *src)
{
	size_t first_len = offsetof(DLGTEMPLATEEX_MOD, menu);
	if(!dst || !src) {
		return NULL;
	}
	ZeroMemory(dst, sizeof(DLGTEMPLATEEX_MOD));
	// Copy everything up to the first mixed-type values
	src = memcpy_advance_src(dst, src, first_len);

	src = sz_or_ord_parse(&dst->menu, (sz_Or_Ord*)src);
	src = sz_or_ord_parse(&dst->windowClass, (sz_Or_Ord*)src);

	src = wcs_assign_advance_src(&dst->title, (const wchar_t*)src);

	if(dst->style & DS_SETFONT) {
		src = memcpy_advance_src(&dst->pointsize, src, sizeof(dst->pointsize));
		src = memcpy_advance_src(&dst->weight, src, sizeof(dst->weight));
		src = memcpy_advance_src(&dst->italic, src, sizeof(dst->italic));
		src = memcpy_advance_src(&dst->charset, src, sizeof(dst->charset));
		src = wcs_assign_advance_src(&dst->typeface, (const wchar_t*)src);
	}
	return (BYTE*)(((UINT_PTR)src + 3) & ~3);
}

size_t dialog_template_ex_rep(DLGTEMPLATEEX_MOD *dst, json_t *trans)
{
	size_t ret = offsetof(DLGTEMPLATEEX_MOD, menu);
	const char *trans_title = strings_get(json_object_get_string(trans, "title"));

	if(!dst) {
		return 0;
	}
	ret += sz_or_ord_rep(&dst->menu, NULL);
	ret += sz_or_ord_rep(&dst->windowClass, NULL);
	ret += sz_or_ord_rep((sz_Or_Ord*)&dst->title, trans_title);

	if(dst->style & DS_SETFONT) {
		ret += sizeof(WORD) + sizeof(WORD) + sizeof(BYTE) + sizeof(BYTE);
		ret += (wcslen(dst->typeface) + 1) * sizeof(wchar_t);
	}
	return (ret + 3) & ~3;
}

BYTE* dialog_template_ex_build(BYTE *dst, const DLGTEMPLATEEX_MOD *src, json_t *trans)
{
	size_t first_len = offsetof(DLGTEMPLATEEX_MOD, menu);
	const char *trans_title = strings_get(json_object_get_string(trans, "title"));

	if(!dst || !src) {
		return NULL;
	}
	dst = memcpy_advance_dst(dst, src, first_len);
	dst = sz_or_ord_build(dst, &src->menu, NULL);
	dst = sz_or_ord_build(dst, &src->windowClass, NULL);
	dst = sz_or_ord_build(dst, (sz_Or_Ord*)&src->title, trans_title);

	if(src->style & DS_SETFONT) {
		dst = memcpy_advance_dst(dst, &src->pointsize, sizeof(src->pointsize));
		dst = memcpy_advance_dst(dst, &src->weight, sizeof(src->weight));
		dst = memcpy_advance_dst(dst, &src->italic, sizeof(src->italic));
		dst = memcpy_advance_dst(dst, &src->charset, sizeof(src->charset));
		dst = memcpy_advance_dst(dst, src->typeface, (wcslen(src->typeface) + 1) * sizeof(wchar_t));
	}
	return (BYTE*)(((UINT_PTR)dst + 3) & ~3);
}

const BYTE* dialog_item_template_ex_parse(DLGITEMTEMPLATEEX_MOD *dst, const BYTE *src)
{
	size_t first_len = offsetof(DLGITEMTEMPLATEEX_MOD, windowClass);
	if(!dst || !src) {
		return NULL;
	}
	ZeroMemory(dst, sizeof(DLGITEMTEMPLATEEX_MOD));
	// Copy everything up to the first mixed-type values
	src = memcpy_advance_src(dst, src, first_len);

	src = sz_or_ord_parse(&dst->windowClass, (sz_Or_Ord*)src);
	src = sz_or_ord_parse(&dst->title, (sz_Or_Ord*)src);

	src = memcpy_advance_src(&dst->extraCount, src, sizeof(dst->extraCount));
	if(dst->extraCount) {
		dst->extraData = src;
		src += dst->extraCount;
	}
	return (BYTE*)(((UINT_PTR)src + 3) & ~3);
}

size_t dialog_item_template_ex_rep(DLGITEMTEMPLATEEX_MOD *dst, json_t *trans)
{
	size_t ret = offsetof(DLGITEMTEMPLATEEX_MOD, windowClass);
	const char *trans_title = strings_get(json_string_value(trans));
	if(!dst) {
		return 0;
	}
	ret += sz_or_ord_rep(&dst->windowClass, NULL);
	ret += sz_or_ord_rep(&dst->title, trans_title);
	ret += sizeof(dst->extraCount);
	ret += dst->extraCount;
	return (ret + 3) & ~3;
}

BYTE* dialog_item_template_ex_build(BYTE *dst, const DLGITEMTEMPLATEEX_MOD *src, json_t *trans)
{
	size_t first_len = offsetof(DLGITEMTEMPLATEEX_MOD, windowClass);
	const char *trans_title = strings_get(json_string_value(trans));
	if(!dst || !src) {
		return NULL;
	}
	dst = memcpy_advance_dst(dst, src, first_len);
	dst = sz_or_ord_build(dst, &src->windowClass, NULL);
	dst = sz_or_ord_build(dst, &src->title, trans_title);

	dst = memcpy_advance_dst(dst, &src->extraCount, sizeof(src->extraCount));
	if(src->extraCount) {
		dst = memcpy_advance_dst(dst, src->extraData, src->extraCount);
	}
	return (BYTE*)(((UINT_PTR)dst + 3) & ~3);
}

void* dialog_translate(HINSTANCE hInstance, LPCSTR lpTemplateName)
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
		const BYTE *src = (BYTE*)hDlg;
		const DLGTEMPLATEEX_MOD *dlg_in = (DLGTEMPLATEEX_MOD*)hDlg;
		json_t *trans_controls = json_object_get(trans, "items");

#ifdef _DEBUG
	Sleep(10000);
#endif
		if(dlg_in->dlgVer == 1 && dlg_in->signature == 0xffff) {
			void *dst;
			DLGTEMPLATEEX_MOD dlg_model;
			DLGITEMTEMPLATEEX_MOD *dlg_item_model = NULL;
			size_t trans_size = 0;
			size_t i;

			src = dialog_template_ex_parse(&dlg_model, src);
			trans_size += dialog_template_ex_rep(&dlg_model, trans);

			dlg_item_model = calloc(dlg_model.cDlgItems, sizeof(DLGITEMTEMPLATEEX_MOD));
			for(i = 0; i < dlg_model.cDlgItems; i++) {
				json_t *trans_item = json_array_get(trans_controls, i);
				src = dialog_item_template_ex_parse(&dlg_item_model[i], src);
				trans_size += dialog_item_template_ex_rep(&dlg_item_model[i], trans_item);
			}

			dst = dlg_out = malloc(trans_size);

			dst = dialog_template_ex_build((BYTE*)dst, &dlg_model, trans);
			for(i = 0; i < dlg_model.cDlgItems; i++) {
				json_t *trans_item = json_array_get(trans_controls, i);
				dst = dialog_item_template_ex_build((BYTE*)dst, &dlg_item_model[i], trans_item);
			}
			SAFE_FREE(dlg_item_model);
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
		ret = CreateDialogParamW(
			hInstance, (LPCWSTR)lpTemplateName, hWndParent, lpDialogFunc, dwInitParam
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
