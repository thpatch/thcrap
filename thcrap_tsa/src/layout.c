/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Text rendering layout.
  */

#include <thcrap.h>
#include "layout.h"

/// Static global variables
/// -----------------------
// Array of absolute tabstop positions.
json_t *Layout_Tabs = NULL;
// Default device context for text rendering; referenced by GetTextExtent
static HDC text_dc = NULL;
/// -----------------------

/// Tokenization
/// ------------
json_t* layout_match(size_t *match_len, const char *str, size_t len)
{
	const char *p = NULL;
	const char *s = NULL; // argument start
	json_t *ret = NULL;
	size_t i = 0;
	int n = 0; // nesting level
	size_t ind = 0;

	if(!str || !len) {
		return ret;
	}
	if(str[0] != '<') {
		return ret;
	}

	ret = json_array();
	s = str + 1;
	for(p = s; (i < len) && (n >= 0); i++, p++) {
		n += (*p == '<');
		n -= (*p == '>');
		if(
			(n == 0 && *p == '$')
			|| (n == -1 && *p == '>')
		) {
			json_array_append_new(ret, json_pack("s#", s, p - s));
			s = p + 1;
		}
	}
	if(match_len) {
		*match_len = s - str;
	}
	return ret;
}

json_t* layout_tokenize(const char *str, size_t len)
{
	json_t *ret;
	size_t i = 0;
	if(!str || !len) {
		return NULL;
	}
	ret = json_array();
	while(i < len) {
		const char *cur_str = str + i;
		size_t cur_len = len - i;

		json_t *match = layout_match(&cur_len, cur_str, cur_len);

		// Requiring at least 2 parameters for a layout command still lets people
		// write "<text>" without that being swallowed by the layout parser.
		if(json_array_size(match) > 1) {
			json_array_append(ret, match);
		} else {
			char *cmd_start = (char*)memchr(cur_str + 1, '<', cur_len - 1);
			if(cmd_start) {
				cur_len = cmd_start - cur_str;
			}
			if(cur_str[0]) {
				char *cur_str_token = EnsureUTF8(cur_str, cur_len);
				json_array_append_new(ret, json_string(cur_str_token));
				SAFE_FREE(cur_str_token);
			}
		}
		json_decref(match);
		i += cur_len;
	}
	return ret;
}
/// ------------

// Outputs the ruby annotation [top_str], relative to [bottom_str] starting at
// [bottom_x], at [top_y] with [hFontRuby] on [hdc]. :)
BOOL layout_textout_ruby(
	__in HDC hdc,
	__in int bottom_x,
	__in const json_t *bottom_str,
	__in int top_y,
	__in const json_t *top_str,
	__in HFONT hFontRuby
) {
	BOOL ret = 0;
	if(json_is_string(bottom_str) && json_is_string(top_str)) {
		size_t bottom_w = GetTextExtentBase(hdc, bottom_str);
		HGDIOBJ hFontOrig = SelectObject(hdc, hFontRuby);
		size_t top_w = GetTextExtentBase(hdc, top_str);
		int top_x = (bottom_w / 2) - (top_w / 2) + bottom_x;

		ret = TextOutU(
			hdc, top_x, top_y, json_string_value(top_str), json_string_length(top_str)
		);

		SelectObject(hdc, hFontOrig);
	}
	return ret;
}

/// Hooked functions
/// ----------------
// Games prior to MoF call this every time a text is rendered.
// However, we require the DC to be present at all times for GetTextExtent.
// Thus, we keep only one DC in memory (the game doesn't need more anyway).
HDC WINAPI layout_CreateCompatibleDC( __in_opt HDC hdc)
{
	if(!text_dc) {
		HDC ret = CreateCompatibleDC(hdc);
		text_dc = ret;
		log_printf("CreateCompatibleDC(0x%8x) -> 0x%8x\n", hdc, ret);
	}
	return text_dc;
}

BOOL WINAPI layout_DeleteDC( __in HDC hdc)
{
	// Bypass this function - we delete our DC on layout_exit()
	return 1;
}

HGDIOBJ WINAPI layout_SelectObject(
	__in HDC hdc,
	__in HGDIOBJ h
	)
{
	if(h == GetStockObject(SYSTEM_FONT)) {
		return GetCurrentObject(hdc, OBJ_FONT);
	} else {
		return SelectObject(hdc, h);
	}
}

BOOL WINAPI layout_TextOutU(
	__in HDC hdc,
	__in int orig_x,
	__in int orig_y,
	__in_ecount(c) LPCSTR lpString,
	__in int c
) {
	HBITMAP hBitmap = (HBITMAP)GetCurrentObject(hdc, OBJ_BITMAP);
	HFONT hFontOrig = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
	HFONT hFontRuby = NULL;

	LOGFONT font_orig;

	BOOL ret = FALSE;
	json_t *tokens;
	json_t *token;
	LONG bitmap_width = 0;

	size_t i = 0;
	int cur_x = orig_x;
	int ruby_y = 0;
	size_t cur_tab = 0;

	if(!lpString || !c) {
		return 0;
	}

	if(c >= strlen(lpString)) {
		lpString = strings_lookup(lpString, &c);
	}

	if(hBitmap) {
		// TODO: This gets the full width of the rendering backbuffer.
		// In-game text rendering mostly only uses a shorter width, though.
		// Find a way to get this width here, too.
		DIBSECTION dibsect;
		GetObject(hBitmap, sizeof(DIBSECTION), &dibsect);
		bitmap_width = dibsect.dsBm.bmWidth;
	}
	if(hFontOrig) {
		// could change after a layout command
		GetObject(hFontOrig, sizeof(LOGFONT), &font_orig);
	}

	tokens = layout_tokenize(lpString, c);

	// Preprocessing
	json_array_foreach(tokens, i, token) {
		if(json_is_array(token)) {
			const char *cmd = json_array_get_string(token, 0);
			// If we have ruby, derive a smaller font from the current one
			// and shift down orig_y
			if(!hFontRuby && strchr(cmd, 'f')) {
				LOGFONT font_ruby;
				memcpy(&font_ruby, &font_orig, sizeof(LOGFONT));
				font_ruby.lfHeight /= 2.25;
				font_ruby.lfWidth /= 2.25;
				font_ruby.lfWeight = 0;
				font_ruby.lfQuality = NONANTIALIASED_QUALITY;
				hFontRuby = CreateFontIndirect(&font_ruby);
				ruby_y = orig_y;
				orig_y += font_ruby.lfHeight / 2.5;
			}
		}
	}

	// Layout!
	json_array_foreach(tokens, i, token) {
		const json_t *draw_str = NULL;
		HFONT hFontNew = NULL;
		size_t cur_w = 0;

		if(json_is_array(token)) {
			const char *cmd = json_array_get_string(token, 0);
			const json_t *p1 = json_array_get(token, 1);
			const json_t *p2 = json_array_get(token, 2);
			const char *p = cmd;
			// Absolute x-end position of the current tab
			size_t tab_end;

			LOGFONT font_new;
			int font_recreate = 0;

			memcpy(&font_new, &font_orig, sizeof(LOGFONT));

			cur_w = GetTextExtentBase(hdc, p1);
			draw_str = p1;

			if(p2) {
				const char *p2_str = json_string_value(p2);
				// Use full bitmap with empty second parameter
				if(p2_str && !p2_str[0]) {
					tab_end = bitmap_width;
					cur_x = orig_x;
				} else {
					tab_end = cur_x + GetTextExtentBase(hdc, p2);
				}
			} else if(cur_tab < json_array_size(Layout_Tabs)) {
				tab_end = json_array_get_hex(Layout_Tabs, cur_tab) + orig_x;
			} else if(json_array_size(Layout_Tabs) > 0) {
				tab_end = bitmap_width;
			} else {
				tab_end = cur_x + cur_w;
			}

			while(*p) {
				switch(p[0]) {
					case 's':
						// Don't actually print anything
						tab_end = cur_w = 0;
						draw_str = NULL;
						break;
					case 't':
						// Tabstop definition
						{
							// The width of the first parameter is already in cur_w, so...
							size_t j = 2;
							const json_t *str_obj = NULL;
							while(str_obj = json_array_get(token, j++)) {
								size_t new_w = GetTextExtentBase(hdc, str_obj);
								cur_w = max(new_w, cur_w);
							}
						}
						tab_end = cur_x + cur_w;
						json_array_set_new_expand(Layout_Tabs, cur_tab, json_integer(tab_end - orig_x));
						break;
					case 'c':
						// Center alignment
						cur_x += ((tab_end - cur_x) / 2) - (cur_w / 2);
						break;
					case 'r':
						// Right alignment
						cur_x = (tab_end - cur_w);
						break;
					case 'b':
						// Bold font
						font_new.lfWeight *= 2;
						font_recreate = 1;
						break;
					case 'i':
						// Italic font
						font_new.lfItalic = TRUE;
						font_recreate = 1;
						break;
					case 'f':
						// Ruby
						layout_textout_ruby(hdc, cur_x, p1, ruby_y, p2, hFontRuby);
						tab_end = 0;
						break;
				}
				p++;
			}
			if(font_recreate) {
				hFontNew = CreateFontIndirect(&font_new);
				SelectObject(hdc, hFontNew);
				tab_end = cur_x + GetTextExtentBase(hdc, draw_str);
			}
			if(tab_end) {
				cur_tab++;
				cur_w = tab_end - cur_x;
			}
		} else if(json_is_string(token)) {
			draw_str = token;
			cur_w = GetTextExtentBase(hdc, token);
		}
		if(draw_str) {
			const char *draw_str_val = json_string_value(draw_str);
			const size_t draw_str_len = json_string_length(draw_str);
			ret = TextOutU(hdc, cur_x, orig_y, draw_str_val, draw_str_len);
		}
		if(hFontNew) {
			SelectObject(hdc, hFontOrig);
			DeleteObject(hFontNew);
			hFontNew = NULL;
		}
		cur_x += cur_w;
	}
	if(hFontRuby) {
		DeleteObject(hFontRuby);
	}
	json_decref(tokens);
	return ret;
}
/// ----------------

size_t GetTextExtentBase(HDC hdc, const json_t *str_obj)
{
	SIZE size = {0};
	const char *str = json_string_value(str_obj);
	const size_t str_len = json_string_length(str_obj);
	GetTextExtentPoint32(hdc, str, str_len, &size);
	return size.cx;
}

size_t __stdcall GetTextExtent(const char *str)
{
	json_t *tokens;
	json_t *token;
	size_t i;
	size_t ret = 0;
	size_t str_len;

	str = strings_lookup(str, &str_len);
	tokens = layout_tokenize(str, str_len);
	json_array_foreach(tokens, i, token) {
		size_t w = 0;
		if(json_is_array(token)) {
			// p1 is the one that's going to be printed.
			// TODO: full layout width calculations all over again?
			w = GetTextExtentBase(text_dc, json_array_get(token, 1));
		} else if(json_is_string(token)) {
			w = GetTextExtentBase(text_dc, token);
		}
		ret += w / 2;
	}
	log_printf("GetTextExtent('%s') = %d\n", str, ret);
	json_decref(tokens);
	return ret;
}

size_t __stdcall GetTextExtentForFont(const char *str, HGDIOBJ font)
{
	HGDIOBJ prev_font = layout_SelectObject(text_dc, font);
	size_t ret = GetTextExtent(str);
	layout_SelectObject(text_dc, prev_font);
	return ret;
}

int layout_init(HMODULE hMod)
{
	Layout_Tabs = json_array();

	return iat_detour_funcs_var(GetModuleHandle(NULL), "gdi32.dll", 4,
		"CreateCompatibleDC", layout_CreateCompatibleDC,
		"DeleteDC", layout_DeleteDC,
		"SelectObject", layout_SelectObject,
		"TextOutA", layout_TextOutU
	);
}

void layout_exit(void)
{
	json_decref(Layout_Tabs);
	if(text_dc) {
		DeleteDC(text_dc);
	}
}
