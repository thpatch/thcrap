/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * GDI-based text rendering layout.
  */

#include <thcrap.h>
#include <unordered_map>
#include <tlnote.hpp>
#include "thcrap_tsa.h"
#include "layout.h"

/// Static global variables
/// -----------------------
// Array of absolute tabstop positions.
json_t *Layout_Tabs = NULL;
// Default device context for text rendering; referenced by GetTextExtent
static HDC text_dc = NULL;
/// -----------------------
static bool ruby_shift_debug = false;
static HBRUSH debug_fill_brush = NULL;

/// Detour chains
/// -------------
typedef HDC WINAPI CreateCompatibleDC_type(
	HDC hdc
);

typedef BOOL WINAPI DeleteObject_type(
	HGDIOBJ obj
);

typedef HGDIOBJ WINAPI SelectObject_type(
	HDC hdc,
	HGDIOBJ h
);

DETOUR_CHAIN_DEF(CreateCompatibleDC);
W32U8_DETOUR_CHAIN_DEF(CreateFont);
W32U8_DETOUR_CHAIN_DEF(GetTextExtentPoint32);
DETOUR_CHAIN_DEF(DeleteObject);
DETOUR_CHAIN_DEF(SelectObject);
W32U8_DETOUR_CHAIN_DEF(TextOut);
auto chain_TextOutW = TextOutW;
auto chain_GetTextExtentPoint32W = GetTextExtentPoint32W;
/// -------------

/// TH06-TH09 font cache
/// --------------------
static std::unordered_map<LONG, HFONT> fontcache;

HFONT fontcache_get(LONG height)
{
	auto stored = fontcache.find(height);
	if(stored == fontcache.end()) {
		LONG weight = (game_id == TH09) ? 400 : (game_id == TH08) ? 600 : 700;
		auto font = chain_CreateFontU(
			height, 0, 0, 0, weight,
			false, false, false,
			SHIFTJIS_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			ANTIALIASED_QUALITY,
			FF_ROMAN | FIXED_PITCH, "MS Gothic"
		);
		return (fontcache[height] = font);
	}
	return stored->second;
}
/// --------------------

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
  *     upper (inclusive) bounds of the block, respectively.
  *
  * • A JSON array containing HFONT pointers to all fonts in the game's ID
  *   order. This can be used if the game doesn't store the pointers in one
  *   contiguous memory block, which makes automatic calculation impossible.
  *
  * For TH06-TH09, this ID simply is the internal text height (before applying
  * the game-specific CreateFont() height formula), which is then pulled from
  * the font cache for these games.
  */
Option<HFONT> font_block_get(int id)
{
	if(game_id >= TH06 && game_id <= TH09) {
		// Simplifies binary hacks if we replicate the game's
		// height -> CreateFont() height formula here.
		return Option<HFONT>(fontcache_get(
			(id * 2) - ((game_id == TH07 || game_id == TH08) ? 2 : 0)
		));
	}
	// TODO: runconfig_json_get returns a const json* and json_object_get_hex can modify the
	// passed value, so technically this is a const violation. See if it's possible to convert
	// all hex strings to JSON5 hex integers so that this isn't a problem.
	json_t *font_block = json_object_get(runconfig_json_get(), "tsa_font_block");
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
			if(id >= min && id <= max) {
				return Option<HFONT>(*(HFONT*)(addr + id * offset));
			}
		} else {
			log_func_printf("invalid TSA font block format\n");
			return Option<HFONT>();
		}
	} else if(json_is_array(font_block)) {
		min = 0;
		max = json_array_size(font_block) - 1;
		return Option<HFONT>(*(HFONT*)json_array_get_hex(font_block, id));
	}
	if(id < min || id > max) {
		log_func_printf(
			"index out of bounds (min: %d, max: %d, given: %d)\n",
			min, max, id
		);
	}
	return Option<HFONT>();
}

Option<HFONT> json_object_get_tsa_font(json_t *object, const char *key)
{
	json_t *val = json_object_get(object, key);
	return
		json_is_string(val) ? Option<HFONT>(*(HFONT*)json_hex_value(val))
		: json_is_integer(val) ? font_block_get((int)json_integer_value(val))
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
size_t BP_ruby_offset(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	char **str_reg = (char**)json_object_get_register(bp_info, regs, "str");
	size_t *offset = json_object_get_register(bp_info, regs, "offset");
	auto font_dialog = json_object_get_tsa_font(bp_info, "font_dialog");
	auto font_ruby = json_object_get_tsa_font(bp_info, "font_ruby");
	char *str = *str_reg;
	// ----------
	if(font_dialog.is_none() || font_ruby.is_none()) {
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
		*offset = ruby_offset_half(
			str_offset, str_base, str_ruby,
			font_dialog.unwrap(), font_ruby.unwrap()
		);
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
// Matches at most [len] bytes of layout markup at [str].
// Returns an array with the layout parameters and, optionally,
// the full length of the layout markup in [str] in [match_len].
json_t* layout_match(size_t &match_len, const char *str, size_t len)
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
	match_len = s - str;
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

		json_t *match = layout_match(cur_len, cur_str, cur_len);

		// Requiring at least 2 parameters (meaning, a single $)
		// for a layout command still lets people write "<text>"
		// without that being swallowed by the layout parser.
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
	tlnote_t tlnote;

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
	return FALSE;
}

BOOL layout_textout_raw_w(layout_state_t *lay, POINT p)
{
	if (lay) {
		const char* str = json_string_value(lay->draw_str);

		WCHAR_T_DEC(str);
		WCHAR_T_CONV(str);
		BOOL ret = chain_TextOutW(lay->hdc, p.x, p.y, str_w, wcslen(str_w));
		WCHAR_T_FREE(str);
		return ret;
	}
	return FALSE;
}

// Modifies [lf] according to the font-related commands in [cmd].
// Returns 1 if anything in [lf] was changed.
int layout_parse_font(LOGFONT &lf, const json_t *token)
{
	int ret = 0;
	const char *cmd = json_array_get_string(token, 0);
	if(cmd) {
		while(*cmd) {
			if(*cmd == 'b') {
				// Bold font
				lf.lfWeight *= 2;
				ret = 1;
			} else if(*cmd == 'i') {
				// Italic font
				lf.lfItalic = true;
				ret = 1;
			} else if(*cmd == 'u') {
				// Underlined font
				lf.lfUnderline = true;
				ret = 1;
			}
			cmd++;
		}
	}
	return ret;
}

// Returns the number of tab commands parsed.
size_t layout_parse_tabs(layout_state_t *lay, const json_t *token)
{
	size_t tabs_count = json_array_size(Layout_Tabs);
	size_t tab_end; // Absolute x-end position of the current tab
	const json_t *cmd_json = json_array_get(token, 0);
	const char *cmd = json_string_value(cmd_json);
	size_t ret = json_string_length(cmd_json);
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
					tab_end = __max(new_w, tab_end);
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

	auto tln = tlnote_find({ str, len }, false);
	str = tln.regular.data();
	len = tln.regular.length();
	lay->tlnote = tln.tlnote;

	if(hBitmap) {
		// TODO: This gets the full width of the text bitmap. The
		// actual visible part of the final text sprites often is
		// shorter, though. Find a way to get this width here, too.
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
			if(layout_parse_font(font_new, token)) {
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
		log_printf("CreateCompatibleDC(0x%p) -> 0x%p\n", hdc, ret);
	}
	return text_dc;
}

HFONT WINAPI fontcache_CreateFontU(
	int cHeight,
	int cWidth,
	int cEscapement,
	int cOrientation,
	int cWeight,
	DWORD bItalic,
	DWORD bUnderline,
	DWORD bStrikeOut,
	DWORD iCharSet,
	DWORD iOutPrecision,
	DWORD iClipPrecision,
	DWORD iQuality,
	DWORD iPitchAndFamily,
	LPCSTR pszFaceName
)
{
	if(game_id >= TH06 && game_id <= TH09) {
		return fontcache_get(cHeight);
	}
	return chain_CreateFontU(
		cHeight, cWidth, cEscapement, cOrientation, cWeight,
		bItalic, bUnderline, bStrikeOut,
		iCharSet,
		iOutPrecision, iClipPrecision,
		iQuality,
		iPitchAndFamily, pszFaceName
	);
}

BOOL WINAPI layout_DeleteDC(HDC hdc)
{
	// Bypass this function - we delete our DC on layout_exit()
	return 1;
}

BOOL WINAPI fontcache_DeleteObject(HGDIOBJ obj)
{
	bool is_font = GetObjectType(obj) == OBJ_FONT;
	if(game_id >= TH06 && game_id <= TH09 && is_font) {
		return 1;
	}
	return chain_DeleteObject(obj);
}

// Since the games themselves would just multiply the half-size ruby offset
// with 2, we capture the actual full-size result of the ruby offset division.
// This admittedly pretty gross hack allows ruby annotations to also be placed
// at odd offsets, effectively gaining twice the precision.
// Negative values would have been nicer, but the TH12.5 mission breakpoints
// already use negative values to signal something else... Bleh.
#define RUBY_OFFSET_DUMMY_HALF (INT16_MAX)
#define RUBY_OFFSET_DUMMY_FULL (RUBY_OFFSET_DUMMY_HALF * 2)

size_t ruby_offset_actual;

void ruby_shift_debug_impl(HDC hdc, int orig_x) {
	json_t* val;
	static auto pen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
	auto hPrevObject = SelectObject(hdc, pen);

	auto draw_at_x = [&](LONG x) {
		auto point = x;
		POINT points[2] = { {point, 0}, {point, 32} };
		Polyline(hdc, points, sizeof(points) / sizeof(points[0]));
	};
	draw_at_x(0);
	draw_at_x(orig_x);
	json_array_foreach_scoped(size_t, i, Layout_Tabs, val) {
		draw_at_x((LONG)json_integer_value(val));
	}
	SelectObject(hdc, hPrevObject);
}


void debug_colorfill_impl(HDC hdc) {
	if unexpected(debug_fill_brush) {
		RECT drawRect = { 0, 0, GetDeviceCaps(hdc, HORZRES), GetDeviceCaps(hdc, VERTRES) };
		FillRect(hdc, &drawRect, debug_fill_brush);
	}
}

BOOL WINAPI layout_TextOutU(
	HDC hdc,
	int orig_x,
	int orig_y,
	LPCSTR lpString,
	int c
) {
	debug_colorfill_impl(hdc);

	// Make sure to keep shadow offsets...
	if(orig_x > (RUBY_OFFSET_DUMMY_FULL / 2)) {
		orig_x = (orig_x - RUBY_OFFSET_DUMMY_FULL) + ruby_offset_actual;
	}

	if (ruby_shift_debug)
		ruby_shift_debug_impl(hdc, orig_x);

	layout_state_t lay = {hdc, {orig_x, orig_y}};
	auto ret = layout_process(&lay, layout_textout_raw, lpString, c);
	tlnote_show(lay.tlnote);
	return ret;
}

BOOL WINAPI layout_TextOutW(
	HDC hdc,
	int orig_x,
	int orig_y,
	LPCWSTR lpString,
	int c
) {
	debug_colorfill_impl(hdc);

	// Make sure to keep shadow offsets...
	if (orig_x > (RUBY_OFFSET_DUMMY_FULL / 2)) {
		orig_x = (orig_x - RUBY_OFFSET_DUMMY_FULL) + ruby_offset_actual;
	}

	if (ruby_shift_debug)
		ruby_shift_debug_impl(hdc, orig_x);

	layout_state_t lay = { hdc, {orig_x, orig_y} };
	UTF8_DEC(lpString);
	UTF8_CONV(lpString);
	auto ret = layout_process(&lay, layout_textout_raw_w, lpString_utf8, lpString_len);
	tlnote_show(lay.tlnote);
	UTF8_FREE(lpString);
	return ret;
}


/// ----------------

size_t GetTextExtentBase(HDC hdc, const json_t *str_obj)
{
	SIZE size = {0};
	const char *str = json_string_value(str_obj);
	const size_t str_len = json_string_length(str_obj);
	chain_GetTextExtentPoint32U(hdc, str, str_len, &size);
	return size.cx;
}

size_t TH_STDCALL text_extent_full(const char *str)
{
	layout_state_t lay = { text_dc };
	STRLEN_DEC(str);
	layout_process(&lay, NULL, str, str_len);
	log_printf("GetTextExtent('%s') = %d\n", str, lay.cur_x / 2);
	return lay.cur_x;
}

BOOL layout_GetTextExtentPoint32A(HDC hdc, LPCSTR lpString, int c, LPSIZE psizl) {
	layout_state_t lay = { hdc };

	BOOL ret1 = chain_GetTextExtentPoint32U(hdc, lpString, c, psizl);
	if (!ret1) {
		return FALSE;
	}

	layout_process(&lay, NULL, lpString, c);
	psizl->cx = lay.cur_x;

	return TRUE;
}

BOOL layout_GetTextExtentPoint32W(HDC hdc, LPCWSTR lpString, int c, LPSIZE psizl) {
	BOOL ret1 = chain_GetTextExtentPoint32W(hdc, lpString, c, psizl);
	if (!ret1) {
		return FALSE;
	}

	size_t lpString_len = c * UTF8_MUL + 1;
	VLA(char, lpString_utf8, lpString_len);
	size_t lpString_utf8_c = StringToUTF8(lpString_utf8, lpString, lpString_len);

	layout_state_t lay = { hdc };
	
	layout_process(&lay, NULL, lpString_utf8, lpString_utf8_c);
	psizl->cx = lay.cur_x;

	UTF8_FREE(lpString);

	return TRUE;
}

size_t TH_STDCALL text_extent_full_for_font(const char *str, HFONT font)
{
	// TH08 doesn't create the DC prior to the first binhacked call of this.
	auto dc = layout_CreateCompatibleDC(nullptr);
	HGDIOBJ prev_font = chain_SelectObject(dc, font);
	size_t ret = text_extent_full(str);
	chain_SelectObject(text_dc, prev_font);
	return ret;
}

TH_DEPRECATED size_t TH_STDCALL GetTextExtent(const char *str)
{
	log_print("WARNING: using deprecated function GetTextExtent\n");
	return text_extent_full(str) / 2;
}

size_t TH_STDCALL GetTextExtentForFont(const char *str, HFONT font)
{
	return text_extent_full_for_font(str, font) / 2;
}

size_t TH_STDCALL GetTextExtentForFontID(const char *str, size_t id)
{
	auto font = font_block_get(id);
	return GetTextExtentForFont(str, font.unwrap_or(nullptr));
}

size_t ruby_offset_half(const char *begin, const char *bottom, const char *ruby, HFONT font_bottom, HFONT font_ruby)
{
	auto offset_double = (text_extent_full_for_font(begin, font_bottom) * 2)
		+ text_extent_full_for_font(bottom, font_bottom)
		- text_extent_full_for_font(ruby, font_ruby);
	// Since we're calculating at 2× the full precision (and 4× the half),
	// adding 1 makes sure that the integer division rounds up at 0.5.
	ruby_offset_actual = (offset_double + 1) / 2;
	// Compensate for the weird shifting of the on-screen X position of
	// the ruby sprite in recent games...
	// The original scripts seem to actually rely on that, so this has
	// to happen here and not in the ANMs.
	if(game_id == TH15 || game_id == TH16) {
		ruby_offset_actual += 4;
	} else if(game_id >= TH165) {
		ruby_offset_actual -= 4;
	}
	return RUBY_OFFSET_DUMMY_HALF;
}

int widest_string(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	json_t *font_id_obj = json_object_get(bp_info, "font_id");
	json_t *strs = json_object_get(bp_info, "strs");
	size_t correction_summand = json_object_get_immediate(bp_info, regs, "correction_summand");
	// ----------
	size_t final_w = 0;
	size_t font_id;
	json_t *val;

	assert(font_id_obj && strs);
	font_id = json_immediate_value(font_id_obj, regs);

	json_flex_array_foreach_scoped(size_t, i, strs, val) {
		const char *str = (const char*)json_immediate_value(val, regs);
		assert(str);
		size_t w = GetTextExtentForFontID(str, font_id) * 2;
		final_w = __max(final_w, w);
	}
	final_w += correction_summand;
	return __max(final_w, 0);
}

#define WIDEST_STRING(name, type) \
	size_t BP_##name##(x86_reg_t *regs, json_t *bp_info) \
	{ \
		type *width = (type *)json_object_get_pointer(bp_info, regs, "width"); \
		assert(width); \
		*width = (type)widest_string(regs, bp_info); \
		return breakpoint_cave_exec_flag(bp_info); \
	}

WIDEST_STRING(widest_string, size_t);
WIDEST_STRING(widest_string_f, float);

int layout_mod_init(HMODULE hMod)
{
	Layout_Tabs = json_array();

	auto* runcfg = runconfig_json_get();
	TH_ASSUME(runcfg);

	json_object_get_eval_bool(runcfg, "ruby_shift_debug", &ruby_shift_debug, JEVAL_DEFAULT);

	size_t col;
	if unexpected(!json_object_get_eval_int(runcfg, "hdc_debug_color", &col, JEVAL_DEFAULT)) {
		// Truncation to the lower 32 bits in 64 bit mode
		debug_fill_brush = CreateSolidBrush((COLORREF)col);
	}

	return 0;
}

void layout_mod_detour(void)
{
	detour_chain("gdi32.dll", 1,
		"CreateCompatibleDC", layout_CreateCompatibleDC, &chain_CreateCompatibleDC,

		// Must be CreateFont rather than CreateFontIndirectEx to
		// continue supporting the legacy "font" runconfig key, as
		// well as font changes via hardcoded string translation.
		"CreateFontA", fontcache_CreateFontU, &chain_CreateFontU,

		"DeleteDC", layout_DeleteDC, NULL,
		"DeleteObject", fontcache_DeleteObject, &chain_DeleteObject,
		"TextOutA", layout_TextOutU, &chain_TextOutU,
		"TextOutW", layout_TextOutW, &chain_TextOutW,

		//"GetTextExtentPoint32W", layout_GetTextExtentPoint32W, &chain_GetTextExtentPoint32W,
		//"GetTextExtentPoint32A", layout_GetTextExtentPoint32A, &chain_GetTextExtentPoint32U,
		NULL
	);

	chain_SelectObject = (decltype(SelectObject)*)detour_top("gdi32.dll", "SelectObject", (FARPROC)SelectObject);
}

void layout_mod_exit(void)
{
	Layout_Tabs = json_decref_safe(Layout_Tabs);
	if(text_dc) {
		DeleteDC(text_dc);
		text_dc = NULL;
	}
	if (debug_fill_brush) {
		DeleteObject(debug_fill_brush);
	}
	for(auto &font : fontcache) {
		DeleteObject((HGDIOBJ)font.second);
	}
	fontcache.clear();
}
