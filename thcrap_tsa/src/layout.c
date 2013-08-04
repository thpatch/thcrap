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
	HBITMAP hBitmap;
	BOOL ret;
	json_t *match = json_array();
	LONG bitmap_width;

	int i = 0;
	int cur_x = orig_x;
	size_t cur_tab = 0;

	hBitmap = GetCurrentObject(hdc, OBJ_BITMAP);
	if(hBitmap) {
		// TODO: This gets the full width of the rendering backbuffer.
		// In-game text rendering mostly only uses a shorter width, though.
		// Find a way to get this width here, too.
		DIBSECTION dibsect;
		GetObject(hBitmap, sizeof(DIBSECTION), &dibsect);
		bitmap_width = dibsect.dsBm.bmWidth;
	}

	while(i < c) {
		LPCSTR cur_str = lpString + i;
		int cur_len = c - i;
		int match_len = layout_match(match, cur_str, cur_len);
		size_t cur_w;
		SIZE str_size;

		// Only do layout processing when everything up to the token has been printed
		if(match_len) {
			const char *cmd = json_array_get_string(match, 0);
			const char *p1 = json_array_get_string(match, 1);
			const char *p2 = json_array_get_string(match, 2);
			json_int_t tab_end;

			GetTextExtentPoint32(hdc, p1, strlen(p1), &str_size);
			cur_w = str_size.cx;

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
				tab_end = json_integer_value(json_array_get(Layout_Tabs, cur_tab));
			} else if(json_array_size(Layout_Tabs) > 0) {
				tab_end = bitmap_width;
			} else {
				tab_end = cur_x + cur_w;
			}

			if(p1) {
				switch(cmd[0]) {
					case 't':
					case 's':
						tab_end = cur_x + cur_w;
						json_array_set_expand(Layout_Tabs, cur_tab, json_integer(tab_end));
						if(cmd[0] == 's') {
							// Don't actually print anything
							cur_w = 0;
							p1 = NULL;
							cur_tab--;
						}
						break;
					case 'c':
						cur_x += ((tab_end - cur_x) / 2) - (cur_w / 2);
						break;
					case 'r':
						cur_x = (tab_end - cur_w);
						break;
				}
				cur_tab++;
				cur_str = p1;
				cur_len = match_len;
				cur_w = tab_end - cur_x;
			}
		}
		// This matches both the standard case without any alignment, as well as
		// the case where the layout markup doesn't contain any parameters.
		// The last one is especially important, as it still lets people write
		// "<text>" without that being swallowed by the layout parser.
		if(!match_len || cur_str == lpString + i) {
			if(cur_str[0] != '<') {
				char *cmd_start = memchr(cur_str, '<', cur_len);
				if(cmd_start) {
					cur_len = cmd_start - cur_str;
				}
			}
			GetTextExtentPoint32(hdc, cur_str, cur_len, &str_size);
			cur_w = str_size.cx;
		}
		ret = TextOutU(hdc, cur_x, orig_y, cur_str, cur_len);

		cur_x += cur_w;
		i += cur_len;
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
