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

// Matches at most [len] bytes of layout markup at the beginning of [str].
// Returns an array with the layout parameters and, optionally,
// the full length of the layout markup in [str] in [match_len].
json_t* layout_match(size_t *match_len, const char *str, size_t len)
{
	const char *end = NULL;
	const char *p = NULL;
	const char *s = NULL; // argument start
	json_t *ret = NULL;
	size_t i = 0;
	size_t ind = 0;

	if(!str || !len) {
		return ret;
	}
	if(str[0] != '<') {
		return ret;
	}

	end = memchr(str, '>', len);
	if(!end) {
		return 0;
	}
	ret = json_array();
	len = end - str;
	s = str + 1;
	for(i = 1, p = s; i < len; i++, p++) {
		if(str[i] == '$') {
			layout_match_set(ret, ind, s, p - s);
			s = p + 1;
			ind++;
		}
	}
	// Append final one
	layout_match_set(ret, ind, s, p - s);
	if(match_len) {
		*match_len = len + 1;
	}
	return ret;
}

// Outputs the ruby annotation [top_str], relative to [bottom_str] starting at
// [bottom_x], at [top_y] with [hFontRuby] on [hdc]. :)
BOOL layout_textout_ruby(
	__in HDC hdc,
	__in int bottom_x,
	__in_ecount(c) LPCSTR bottom_str,
	__in int top_y,
	__in_ecount(c) LPCSTR top_str,
	__in HFONT hFontRuby
) {
	if(!bottom_str || !top_str) {
		return 0;
	}
	{
		HFONT hFontOrig;
		SIZE str_size;
		size_t bottom_w;
		size_t top_w;
		int top_x;
		size_t bottom_len = strlen(bottom_str) + 1;
		size_t top_len = strlen(top_str) + 1;
		BOOL ret;

		GetTextExtentPoint32(hdc, bottom_str, bottom_len, &str_size);
		bottom_w = str_size.cx;

		hFontOrig = SelectObject(hdc, hFontRuby);

		GetTextExtentPoint32(hdc, top_str, top_len, &str_size);
		top_w = str_size.cx;

		top_x = (bottom_w / 2) - (top_w / 2) + bottom_x;

		ret = TextOutU(hdc, top_x, top_y, top_str, top_len);

		SelectObject(hdc, hFontOrig);
		return ret;
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

	BOOL ret;
	json_t *tokens;
	json_t *token;
	LONG bitmap_width;

	int i = 0;
	int cur_x = orig_x;
	int ruby_y;
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

	tokens = json_array();

	// Split the string into an array of tokens to render in a sequence.
	// These are either strings (= direct text) or arrays in itself (= layout commands).
	while(i < c) {
		LPCSTR cur_str = lpString + i;
		size_t cur_len = c - i;

		json_t *match = layout_match(&cur_len, cur_str, cur_len);

		// Requiring at least 2 parameters for a layout command still lets people
		// write "<text>" without that being swallowed by the layout parser.
		if(json_array_size(match) > 1) {
			json_array_append_new(tokens, match);
		} else {
			char *cmd_start = (char*)memchr(cur_str + 1, '<', cur_len - 1);
			if(cmd_start) {
				cur_len = cmd_start - cur_str;
			}
			{
				char *cur_str_token = EnsureUTF8(cur_str, cur_len);
				json_array_append_new(tokens, json_string(cur_str_token));
				SAFE_FREE(cur_str_token);
			}
		}
		i += cur_len;
	}

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
		const char *draw_str = NULL;
		HFONT hFontNew = NULL;
		size_t cur_w = 0;
		SIZE str_size;

		if(json_is_array(token)) {
			const char *cmd = json_array_get_string(token, 0);
			const char *p1 = json_array_get_string(token, 1);
			const char *p2 = json_array_get_string(token, 2);
			const char *p = cmd;
			// Absolute x-end position of the current tab
			size_t tab_end;
			// We're guaranteed to have at least p1 if we come here
			size_t p1_len = strlen(p1) + 1;

			LOGFONT font_new;
			int font_recreate = 0;

			memcpy(&font_new, &font_orig, sizeof(LOGFONT));

			GetTextExtentPoint32(hdc, p1, p1_len, &str_size);
			cur_w = str_size.cx;
			draw_str = p1;

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
							const char *str = NULL;
							for(; j < json_array_size(token) && (str = json_array_get_string(token, j)); j++) {
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
				GetTextExtentPoint32(hdc, p1, p1_len, &str_size);
				tab_end = cur_x + str_size.cx;
			}
			if(tab_end) {
				cur_tab++;
				cur_w = tab_end - cur_x;
			}
		} else if(json_is_string(token)) {
			draw_str = json_string_value(token);
			GetTextExtentPoint32(hdc, draw_str, strlen(draw_str), &str_size);
			cur_w = str_size.cx;
		}
		if(draw_str) {
			ret = TextOutU(hdc, cur_x, orig_y, draw_str, strlen(draw_str));
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
