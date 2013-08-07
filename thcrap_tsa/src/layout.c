/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Text rendering layout.
  */

#include <thcrap.h>
#include <textdisp.h>

/// Static global variables
/// -----------------------
// Array of absolute tabstop positions.
json_t *Layout_Tabs = NULL;
/// -----------------------

int layout_match_set(json_t *arr, size_t ind, const char *str, size_t len)
{
	// We explicitly _don't_ check for len == 0 here!
	if(!json_is_array(arr) || !str) {
		return -1;
	}
	{
		VLA(char, arg, len + 1);
		json_t *json_str;

		strncpy(arg, str, len);
		arg[len] = 0;
		json_str = json_string(arg);

		return json_array_set_expand(arr, ind, json_str);
	}
}

// Matches at most [len] bytes of layout markup at the beginning of [str],
// storing parameters in [arr].
// Returns the full length of the layout markup in the original string.
size_t layout_match(json_t *arr, const char *str, size_t len)
{
	const char *end = NULL;
	const char *p = NULL;
	const char *s = NULL; // argument start
	size_t i = 0;
	size_t ind = 0;

	if(!json_is_array(arr) || !str || !len) {
		return 0;
	}
	if(str[0] != '<') {
		return 0;
	}

	end = memchr(str, '>', len);
	if(!end) {
		return 0;
	}
	len = end - str;
	s = str + 1;
	for(i = 1, p = s; i < len; i++, p++) {
		if(str[i] == '$') {
			layout_match_set(arr, ind, s, p - s);
			s = p + 1;
			ind++;
		}
	}
	// Append final one
	layout_match_set(arr, ind, s, p - s);
	return len + 1;
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
	HFONT hFontNew = NULL;

	BOOL ret;
	json_t *match = json_array();
	LONG bitmap_width;

	int i = 0;
	int cur_x = orig_x;
	size_t cur_tab = 0;
	int font_recreate = 0;

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

	while(i < c) {
		LPCSTR draw_str = lpString + i;
		int draw_str_len = c - i;
		int advance_len = draw_str_len;
		int match_len;
		size_t cur_w = 0;
		SIZE str_size;

		json_array_clear(match);
		match_len = layout_match(match, draw_str, draw_str_len);
		// Only do layout processing when everything up to the token has been printed
		if(match_len) {
			const char *cmd = json_array_get_string(match, 0);
			const char *p1 = json_array_get_string(match, 1);
			const char *p2 = json_array_get_string(match, 2);
			// Absolute x-end position of the current tab
			size_t tab_end;

			if(p1) {
				advance_len = match_len;
				draw_str_len = strlen(p1) + 1;

				GetTextExtentPoint32(hdc, p1, draw_str_len, &str_size);
				cur_w = str_size.cx;
			}

			if(p2) {
				// Use full bitmap with empty second parameter
				if(!p2[0]) {
					tab_end = bitmap_width;
					cur_x = orig_x;
				} else {
					GetTextExtentPoint32(hdc, p2, strlen(p2), &str_size);
					tab_end = cur_x + str_size.cx;
				}
			} else if(cur_tab < json_array_size(Layout_Tabs)) {
				tab_end = json_integer_value(json_array_get(Layout_Tabs, cur_tab)) + orig_x;
			} else if(json_array_size(Layout_Tabs) > 0) {
				tab_end = bitmap_width;
			} else {
				tab_end = cur_x + cur_w;
			}

			if(p1 && cmd) {
				const char *p = cmd;
				LOGFONT font;

				GetObject(hFontOrig, sizeof(LOGFONT), &font);
				while(*p) {
					switch(p[0]) {
						case 's':
							// Don't actually print anything
							tab_end = cur_w = 0;
							p1 = NULL;
							break;
						case 't':
							// Tabstop definition
							{
								// The width of the first parameter is already in cur_w, so...
								size_t j = 2;
								const char *str = NULL;
								for(; j < json_array_size(match) && (str = json_array_get_string(match, j)); j++) {
									GetTextExtentPoint32(hdc, str, strlen(str), &str_size);
									cur_w = max(str_size.cx, cur_w);
								}
							}
							tab_end = cur_x + cur_w;
							json_array_set_expand(Layout_Tabs, cur_tab, json_integer(tab_end - orig_x));
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
							font.lfWeight *= 2;
							font_recreate = 1;
							break;
						case 'i':
							// Italic font
							font.lfItalic = TRUE;
							font_recreate = 1;
							break;
					}
					p++;
				}
				if(font_recreate) {
					hFontNew = CreateFontIndirect(&font);
					SelectObject(hdc, hFontNew);
					GetTextExtentPoint32(hdc, p1, draw_str_len, &str_size);
					tab_end = cur_x + str_size.cx;
				}
				if(tab_end) {
					cur_tab++;
					cur_w = tab_end - cur_x;
				}
				draw_str = p1;
			}
		}
		// This matches both the standard case without any alignment, as well as
		// the case where the layout markup doesn't contain any parameters.
		// The last one is especially important, as it still lets people write
		// "<text>" without that being swallowed by the layout parser.
		if(!match_len || draw_str == lpString + i) {
			char *cmd_start = (char*)memchr(draw_str + 1, '<', advance_len - 1);
			if(cmd_start) {
				draw_str_len = advance_len = cmd_start - draw_str;
			}
			GetTextExtentPoint32(hdc, draw_str, draw_str_len, &str_size);
			cur_w = str_size.cx;
		}
		ret = TextOutU(hdc, cur_x, orig_y, draw_str, draw_str_len);

		if(font_recreate) {
			SelectObject(hdc, hFontOrig);
			DeleteObject(hFontNew);
			hFontNew = NULL;
		}
		cur_x += cur_w;
		i += advance_len;
	}
	json_decref(match);
	return ret;
}

void layout_init(HMODULE hMod)
{
	Layout_Tabs = json_array();

	iat_patch_funcs_var(GetModuleHandle(NULL), "gdi32.dll", 1,
		"TextOutA", layout_TextOutU
	);
}

void layout_exit()
{
	json_decref(Layout_Tabs);
}
