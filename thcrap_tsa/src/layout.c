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

/// Detour chains
/// -------------
typedef HDC WINAPI CreateCompatibleDC_type(
	HDC hdc
);

typedef HGDIOBJ WINAPI SelectObject_type(
	HDC hdc,
	HGDIOBJ h
);

DETOUR_CHAIN_DEF(CreateCompatibleDC);
DETOUR_CHAIN_DEF(SelectObject);
W32U8_DETOUR_CHAIN_DEF(TextOut);
/// -------------

/// TSA font block
/// --------------
/**
  * Retrieves a HFONT pointer using the TSA font block defined in the run
  * configuration. This font block can be defined using two methods:
  *
  * • A JSON object with 4 members:
  *   • "addr" (hex) for the start address
  *   • "offset" (int, defaults to sizeof(HFONT)) for the number of bytes
  *     between the individual font pointers
  *   • "min" (int, defaults to 0) and "max" (int) to define the lower and
  *     upper bounds of the block, respectively.
  *
  * • A JSON array containing HFONT pointers to all fonts in the game's ID
  *   order. This can be used if the game doesn't store the pointers in one
  *   contiguous memory block, which makes automatic calculation impossible.
  */
static HFONT* font_block_get(int id)
{
	HFONT* ret = NULL;
	const json_t *run_cfg = runconfig_get();
	json_t *font_block = json_object_get(run_cfg, "tsa_font_block");
	json_int_t min = 0, max = 0;
	if(json_is_object(font_block)) {
		size_t addr = json_object_get_hex(font_block, "addr");
		const json_t *block_offset = json_object_get(font_block, "offset");
		const json_t *block_min = json_object_get(font_block, "min");
		const json_t *block_max = json_object_get(font_block, "max");

		json_int_t offset = sizeof(HFONT);
		if(json_is_integer(block_offset)) {
			offset = json_integer_value(block_offset);
		}
		if(json_is_integer(block_min) && json_is_integer(block_max) && addr) {
			min = json_integer_value(block_min);
			max = json_integer_value(block_max);
			if(id >= min && id < max) {
				ret = (HFONT*)(addr + id * offset);
			}
		} else {
			log_func_printf("invalid TSA font block format\n");
			return NULL;
		}
	} else if(json_is_array(font_block)) {
		min = 0;
		max = json_array_size(font_block);
		ret = (HFONT*)json_array_get_hex(font_block, id);
	}
	if(id < min || id > max) {
		log_func_printf(
			"index out of bounds (min: %d, max: %d, given: %d)\n",
			min, max, id
		);
	}
	return ret;
}

static HFONT* json_object_get_tsa_font(json_t *object, const char *key)
{
	json_t *val = json_object_get(object, key);
	return
		json_is_string(val) ? (HFONT*)json_hex_value(val)
		: json_is_integer(val) ? font_block_get(json_integer_value(val))
		: NULL
	;
}
/// --------------

/// Ruby
/// ----

char *strchr_backwards(char *str, char c)
{
	while(*(--str) != c);
	return str;
}
/**
  * Calculates the X [offset] of a Ruby annotation centered over a section of
  * text, when [font_dialog] is the font used for regular dialog text, and
  * [font_ruby] is the font used for the annotation itself.
  * The th06 MSG patcher already guarantees that we got valid TSA ruby syntax
  * in [str] when we get here. Leveraging the original system, this breakpoint
  * then expects the following syntax in [str]:
  *
  *		|\tBase text, \t,\tsection to annotate.\t,Annotation text
  *
  * The additional \t is used to have a better delimiter for the parameters.
  * At the beginning, it makes sure that the game's own atoi() call returns 0,
  * even if a parameter starts with an ASCII digit, while its use at the end
  * allows ASCII commas inside the parameters.
  */
int BP_ruby_offset(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	char **str_reg = (char**)json_object_get_register(bp_info, regs, "str");
	size_t *offset = json_object_get_register(bp_info, regs, "offset");
	HFONT* font_dialog = json_object_get_tsa_font(bp_info, "font_dialog");
	HFONT* font_ruby = json_object_get_tsa_font(bp_info, "font_ruby");
	char *str = *str_reg;
	// ----------
	if(!font_dialog || !font_ruby) {
		log_func_printf(
			"Missing \"font_dialog\" or \"font_ruby\" parameter, skipping..."
		);
		return 1;
	}
	if(str && str[0] == '\t' && offset) {
		char *str_offset = str + 1;
		char *str_offset_end = strchr(str_offset, '\t');
		char *str_base = str_offset_end + 3;
		char *str_base_end = NULL;
		char *str_ruby = NULL;

		if(!str_offset_end || str_offset_end[1] != ',' || str_base[-1] != '\t') {
			return 1;
		}
		str_base_end = strchr(str_base, '\t');
		str_ruby = str_base_end + 2;
		if(!str_base_end || str_base_end[1] != ',') {
			return 1;
		}
		*str_offset_end = '\0';
		*str_base_end = '\0';
		*offset =
			GetTextExtentForFont(str_offset, *font_dialog)
			+ (GetTextExtentForFont(str_base, *font_dialog) / 2)
			- (GetTextExtentForFont(str_ruby, *font_ruby) / 2);
		*str_offset_end = '\t';
		*str_base_end = '\t';

		// In case any of our parameters included a comma, adjust the original string
		// pointer to point to the second comma before the annotation, so that the
		// stupid game actually gets the right text after its two strchr() calls.
		*str_reg = strchr_backwards(str_base_end, ',');
	}
	return 1;
}
/// ----

/// Tokenization
/// ------------
json_t* layout_match(size_t *match_len, const char *str, size_t len)
{
	size_t i = 1;
	const char *p = NULL;
	const char *s = str + i; // argument start
	json_t *ret = NULL;
	int n = 0; // nesting level

	if(!str || !len) {
		return ret;
	}
	if(str[0] != '<') {
		return ret;
	}

	ret = json_array();
	for(p = s; (i < len) && (n >= 0); i++, p++) {
		n += (*p == '<');
		n -= (*p == '>');
		if(
			(n == 0 && *p == '$')
			|| (n == -1 && *p == '>')
		) {
			json_array_append_new(ret, json_stringn_nocheck(s, p - s));
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
				json_array_append_new(ret, json_stringn_nocheck(cur_str, cur_len));
			}
		}
		json_decref(match);
		i += cur_len;
	}
	return ret;
}
/// ------------

/// Layout processing
/// -----------------
typedef struct {
	/// Environment
	HDC hdc;
	POINT orig; // Drawing origin
	LONG bitmap_width;

	/// State
	json_t *tokens;
	size_t cur_tab;
	// After layout processing completed, this contains
	// the total rendered width of the full string.
	int cur_x;

	/// Current pass
	size_t token_id;
	json_t *draw_str; // String to draw
	size_t cur_w; // Amount of pixels to advance [cur_x] after drawing
} layout_state_t;

// A function to be called for every substring to be rendered.
// [lay->draw_str] points to the string, [p] is the absolute drawing position.
typedef BOOL (*layout_func_t)(layout_state_t *lay, POINT p);

BOOL layout_textout_raw(layout_state_t *lay, POINT p)
{
	if(lay) {
		const char *str = json_string_value(lay->draw_str);
		const size_t len = json_string_length(lay->draw_str);
		return chain_TextOutU(lay->hdc, p.x, p.y, str, len);
	}
	return 0;
}

// Modifies [lf] according to the font-related commands in [cmd].
// Returns 1 if anything in [lf] was changed.
int layout_parse_font(LOGFONT *lf, const json_t *token)
{
	int ret = 0;
	const char *cmd = json_array_get_string(token, 0);
	if(lf && cmd) {
		while(*cmd) {
			if(*cmd == 'b') {
				// Bold font
				lf->lfWeight *= 2;
				ret = 1;
			} else if(*cmd == 'i') {
				// Italic font
				lf->lfItalic = TRUE;
				ret = 1;
			}
			cmd++;
		}
	}
	return ret;
}

// Returns the number of tab commands parsed.
int layout_parse_tabs(layout_state_t *lay, const json_t *token)
{
	size_t tabs_count = json_array_size(Layout_Tabs);
	size_t tab_end; // Absolute x-end position of the current tab
	const json_t *cmd_json = json_array_get(token, 0);
	const char *cmd = json_string_value(cmd_json);
	int ret = json_string_length(cmd_json);
	const json_t *p2 = json_array_get(token, 2);
	if(p2) {
		const char *p2_str = json_string_value(p2);
		// Use full bitmap with empty second parameter
		if(p2_str && !p2_str[0]) {
			tab_end = lay->bitmap_width;
		} else {
			tab_end = lay->cur_x + GetTextExtentBase(lay->hdc, p2);
		}
	} else if(lay->cur_tab < tabs_count) {
		tab_end = json_array_get_hex(Layout_Tabs, lay->cur_tab);
	} else if(tabs_count > 0 && lay->token_id == (json_array_size(lay->tokens) - 1)) {
		tab_end = lay->bitmap_width;
	} else {
		tab_end = lay->cur_x + lay->cur_w;
	}

	while(*cmd) {
		size_t j = 2;
		const json_t *str_obj = NULL;
		switch(cmd[0]) {
			case 's':
				// Don't actually print anything
				tab_end = lay->cur_x;
				lay->draw_str = NULL;
				break;
			case 't':
				// Tabstop definition
				// The width of the first parameter is already in cur_w, so...
				tab_end = lay->cur_w;
				while(str_obj = json_array_get(token, j++)) {
					size_t new_w = GetTextExtentBase(lay->hdc, str_obj);
					tab_end = max(new_w, tab_end);
				}
				tab_end += lay->cur_x;
				json_array_set_new_expand(Layout_Tabs, lay->cur_tab, json_integer(tab_end));
				break;
			case 'l':
				// Left alignment
				// Does nothing, but shouldn't hit the default case.
				break;
			case 'c':
				// Center alignment
				lay->cur_x += ((tab_end - lay->cur_x) / 2) - (lay->cur_w / 2);
				break;
			case 'r':
				// Right alignment
				lay->cur_x = (tab_end - lay->cur_w);
				break;
			default:
				ret--;
				break;
		}
		cmd++;
	}
	if(ret) {
		lay->cur_tab++;
		lay->cur_w = tab_end - lay->cur_x;
	}
	return 1;
}

// Performs layout of the string [str] with length [len]. [lay] needs to be
// initialized with the HDC and the origin point before calling this function.
// [func] is called for each substring to be rendered.
int layout_process(layout_state_t *lay, layout_func_t func, const char *str, size_t len)
{
	int ret = 1;
	HBITMAP hBitmap;
	HFONT hFontOrig;
	LOGFONT font_orig;
	json_t *token;
	if(!lay || !lay->hdc || !str || !len) {
		return -1;
	}
	hBitmap = (HBITMAP)GetCurrentObject(lay->hdc, OBJ_BITMAP);
	hFontOrig = (HFONT)GetCurrentObject(lay->hdc, OBJ_FONT);

	if(len >= strlen(str)) {
		str = strings_lookup(str, &len);
	}

	if(hBitmap) {
		// TODO: This gets the full width of the rendering backbuffer.
		// In-game text rendering mostly only uses a shorter width, though.
		// Find a way to get this width here, too.
		DIBSECTION dibsect;
		GetObject(hBitmap, sizeof(DIBSECTION), &dibsect);
		lay->bitmap_width = dibsect.dsBm.bmWidth;
	}
	if(hFontOrig) {
		// could change after a layout command
		GetObject(hFontOrig, sizeof(LOGFONT), &font_orig);
	}

	lay->tokens = layout_tokenize(str, len);

	json_array_foreach(lay->tokens, lay->token_id, token) {
		HFONT hFontNew = NULL;
		if(json_is_array(token)) {
			LOGFONT font_new = font_orig;
			lay->draw_str = json_array_get(token, 1);

			// If requested, derive a bold/italic font from the current one.
			// This is done before evaluating tab commmands to guarantee that
			// any calls to GetTextExtent() return the correct widths.
			if(layout_parse_font(&font_new, token)) {
				hFontNew = CreateFontIndirectW(&font_new);
				SelectObject(lay->hdc, hFontNew);
			}

			lay->cur_w = GetTextExtentBase(lay->hdc, lay->draw_str);
			layout_parse_tabs(lay, token);
		} else if(json_is_string(token)) {
			lay->draw_str = token;
			lay->cur_w = GetTextExtentBase(lay->hdc, lay->draw_str);
		}
		if(lay->draw_str && func) {
			POINT p = {lay->orig.x + lay->cur_x, lay->orig.y};
			ret = func(lay, p);
		}
		if(hFontNew) {
			SelectObject(lay->hdc, hFontOrig);
			DeleteObject(hFontNew);
			hFontNew = NULL;
		}
		lay->cur_x += lay->cur_w;
	}
	lay->tokens = json_decref_safe(lay->tokens);
	return ret;
}
/// -----------------

/// Hooked functions
/// ----------------
// Games prior to MoF call this every time a text is rendered.
// However, we require the DC to be present at all times for GetTextExtent.
// Thus, we keep only one DC in memory (the game doesn't need more anyway).
HDC WINAPI layout_CreateCompatibleDC(HDC hdc)
{
	if(!text_dc) {
		HDC ret = (HDC)chain_CreateCompatibleDC(hdc);
		text_dc = ret;
		log_printf("CreateCompatibleDC(0x%8x) -> 0x%8x\n", hdc, ret);
	}
	return text_dc;
}

BOOL WINAPI layout_DeleteDC(HDC hdc)
{
	// Bypass this function - we delete our DC on layout_exit()
	return 1;
}

HGDIOBJ WINAPI layout_SelectObject(
	HDC hdc,
	HGDIOBJ h
)
{
	if(h == GetStockObject(SYSTEM_FONT)) {
		return GetCurrentObject(hdc, OBJ_FONT);
	} else {
		return (HGDIOBJ)chain_SelectObject(hdc, h);
	}
}

BOOL WINAPI layout_TextOutU(
	HDC hdc,
	int orig_x,
	int orig_y,
	LPCSTR lpString,
	int c
) {
	layout_state_t lay = {hdc, {orig_x, orig_y}};
	return layout_process(&lay, layout_textout_raw, lpString, c);
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
	size_t ret = 0;
	layout_state_t lay = {text_dc};
	STRLEN_DEC(str);
	layout_process(&lay, NULL, str, str_len);
	ret = lay.cur_x /= 2;
	log_printf("GetTextExtent('%s') = %d\n", str, ret);
	return ret;
}

size_t __stdcall GetTextExtentForFont(const char *str, HFONT font)
{
	HGDIOBJ prev_font = layout_SelectObject(text_dc, font);
	size_t ret = GetTextExtent(str);
	layout_SelectObject(text_dc, prev_font);
	return ret;
}

size_t __stdcall GetTextExtentForFontID(const char *str, size_t id)
{
	HFONT *font = font_block_get(id);
	return GetTextExtentForFont(str, font ? *font : NULL);
}

int layout_mod_init(HMODULE hMod)
{
	Layout_Tabs = json_array();
	return 0;
}

void layout_mod_detour(void)
{
	detour_chain("gdi32.dll", 1,
		"CreateCompatibleDC", layout_CreateCompatibleDC, &chain_CreateCompatibleDC,
		"DeleteDC", layout_DeleteDC, NULL,
		"SelectObject", layout_SelectObject, &chain_SelectObject,
		"TextOutA", layout_TextOutU, &chain_TextOutU,
		NULL
	);
}

void layout_mod_exit(void)
{
	Layout_Tabs = json_decref_safe(Layout_Tabs);
	if(text_dc) {
		DeleteDC(text_dc);
		text_dc = NULL;
	}
}
