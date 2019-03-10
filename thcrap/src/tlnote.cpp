/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * On-screen display of translation notes.
  */

#include "thcrap.h"
#include <vector>
#include "minid3d.h"
#include "textdisp.h"
#include "tlnote.hpp"

/// Codepoints
/// ----------
/// All of these are UTF-8-encoded.

// Separates in-line TL note text (after) from regular text (before).
// This is the codepoint that should be used by patch authors.
#define TLNOTE_INLINE 0x14

// Indicates that the next index is a 1-based, UTF-8-encoded internal index of
// a previously rendered TL note. Can appear either at the beginning or the
// end of a string.
#define TLNOTE_INDEX 0x12
/// ----------

/// Error reporting and debugging
/// -----------------------------
logger_t tlnote_log("TL note error");
/// -----------------------------

/// Structures
/// ----------
tlnote_t::tlnote_t(const stringref_t str)
	: str((tlnote_string_t *)str.str), len(str.len - 1) {
	assert(str.str[0] == TLNOTE_INLINE || str.str[0] == TLNOTE_INDEX);
	assert(str.len > 1);
}
/// ----------

/// Rendering values
/// ----------------
void font_delete(HFONT font)
{
	if(!font) {
		return;
	}
	typedef BOOL WINAPI DeleteObject_type(
		HGDIOBJ obj
	);
	((DeleteObject_type *)detour_top(
		"gdi32.dll", "DeleteObject", (FARPROC)DeleteObject
	))(font);
}

THCRAP_API bool tlnote_env_render_t::reference_resolution_set(const vector2_t &newval)
{
	if(newval.x <= 0 || newval.y <= 0) {
		tlnote_log.errorf(
			"Reference resolution must be positive and nonzero, got %f\xE2\x9C\x95%f",
			newval.x, newval.y
		);
		return false;
	}
	_reference_resolution = newval;
	return true;
}

THCRAP_API bool tlnote_env_render_t::font_set(const LOGFONTA &lf)
{
	font_delete(_font);
	// Must be CreateFont() rather than CreateFontIndirect() to
	// continue supporting the legacy "font" runconfig key.
	_font = ((CreateFontA_type *)detour_top(
		"gdi32.dll", "CreateFontA", (FARPROC)CreateFontU
	))(
		lf.lfHeight, lf.lfWidth, lf.lfEscapement, lf.lfOrientation,
		lf.lfWeight, lf.lfItalic, lf.lfUnderline, lf.lfStrikeOut,
		lf.lfCharSet, lf.lfOutPrecision, lf.lfClipPrecision,
		lf.lfQuality, lf.lfPitchAndFamily, lf.lfFaceName
	);
	return true;
}

THCRAP_API bool tlnote_env_t::region_size_set(const vector2_t &newval)
{
	const auto &rr = _reference_resolution;
	if(
		newval.x <= 0 || newval.y <= 0 || newval.x >= rr.x || newval.y >= rr.y
	) {
		tlnote_log.errorf(
			"Region size must be nonzero and smaller than the reference resolution (%f\xE2\x9C\x95%f), got %f\xE2\x9C\x95%f",
			rr.x, rr.y, newval.x, newval.y
		);
		return false;
	}
	_region_w = newval.x;
	region_h = newval.y;
	return true;
}

bool tlnote_env_from_runconfig(tlnote_env_t &env)
{
	auto fail = [] (const char *context, const char *err) {
		tlnote_log.errorf("{\"tlnotes\": {\"%s\"}}: %s", context, err);
		return false;
	};

#define PARSE(var, func, fail_check, on_fail, on_success) { \
	auto json_value = json_object_get(cfg, #var); \
	if(json_value) { \
		auto parsed = func(json_value); \
		if(fail_check) on_fail else on_success \
	} \
}

#define PARSE_TUPLE(var, func, on_success) \
	PARSE(var, func, (!parsed.err.empty()), { \
		fail(#var, parsed.err.c_str()); \
	}, on_success)

	auto cfg = json_object_get(runconfig_get(), "tlnotes");
	if(!cfg) {
		return true;
	}

	bool ret = true;

	PARSE_TUPLE(reference_resolution, json_vector2_value, {
		ret &= env.reference_resolution_set(parsed.v);
	});
	PARSE_TUPLE(region_topleft, json_vector2_value, {
		env.region_left = parsed.v.x;
		env.region_top = parsed.v.y;
	});
	PARSE_TUPLE(region_size, json_vector2_value, {
		ret &= env.region_size_set(parsed.v);
	});
	PARSE(font, json_string_value, (parsed == nullptr), {
		fail("font", "Must be a font rule string.");
	}, {
		LOGFONTA lf = {0};
		fontrule_parse(&lf, parsed);
		ret &= env.font_set(lf);
	});

#undef PARSE_TUPLE
#undef PARSE

	return ret;
}

tlnote_env_t& tlnote_env()
{
	static struct tlnote_env_static_wrap_t {
		tlnote_env_t env;

		// TODO: We might want to continuously nag the patch author once
		// we actually make the runconfig repatchable, but for now...
		tlnote_env_static_wrap_t() {
			tlnote_env_from_runconfig(env);
		}
		~tlnote_env_static_wrap_t() {
			font_delete(env.font());
		}
	} env_wrap;
	return env_wrap.env;
}
/// ----------------

/// Direct3D pointers
/// -----------------
// Game support modules might want to pre-render TL notes before the game
// created its IDirect3DDevice instance.
d3d_version_t d3d_ver = D3D_NONE;
IDirect3DDevice *d3dd = nullptr;
/// -----------------

/// Texture creation
/// ----------------
struct tlnote_rendered_t {
	// "Primary key" data
	// -------------------
	std::string note;
	tlnote_env_render_t render_env;
	// -------------------

	// Runtime stuff
	// -------------
	IDirect3DTexture *tex = nullptr;
	// -------------

	IDirect3DTexture* render(d3d_version_t ver, IDirect3DDevice *d3dd);

	bool matches(const stringref_t &n2, const tlnote_env_render_t &r2) const {
		return
			(note.size() == n2.len)
			&& (render_env == r2)
			&& (note.compare(n2.str) == 0);
	}

	tlnote_rendered_t(const stringref_t &note, tlnote_env_render_t render_env)
		: note({ note.str, (size_t)note.len }), render_env(render_env) {
	}
};

IDirect3DTexture* tlnote_rendered_t::render(d3d_version_t ver, IDirect3DDevice *d3dd)
{
	if(tex) {
		return tex;
	}
	return tex;
}
/// ----------------

/// Indexing of rendered TL notes
/// -----------------------------
std::vector<tlnote_rendered_t> rendered;

#define SURROGATE_START (0xD800)
#define SURROGATE_END   (0xE000)
#define SURROGATE_COUNT (SURROGATE_END - SURROGATE_START)

// Encoded index that maps to the first entry in [rendered]
#define RENDERED_OFFSET 1
// Excluding '\0' as well as the UTF-16 surrogate halves (how nice of us!)
#define RENDERED_MAX (0x10FFFF - SURROGATE_COUNT)

// These basically follow the UTF-8 spec, except that the index can include
// the range of UTF-16 surrogate halves.
tlnote_encoded_index_t::tlnote_encoded_index_t(int32_t index)
{
	assert(index >= 1 && index <= RENDERED_MAX);
	type = TLNOTE_INDEX;
	if(index >= SURROGATE_START) {
		index += SURROGATE_COUNT;
	}

	if(index < 0x80) {
		index_as_utf8[0] = (char)index;
		index_len = 1;
	} else if(index < 0x800) {
		index_as_utf8[0] = 0xC0 + ((index & 0x7C0) >> 6);
		index_as_utf8[1] = 0x80 + ((index & 0x03F));
		index_len = 2;
	} else if(index < 0x10000) {
		index_as_utf8[0] = 0xE0 + ((index & 0xF000) >> 12);
		index_as_utf8[1] = 0x80 + ((index & 0x0FC0) >> 6);
		index_as_utf8[2] = 0x80 + ((index & 0x003F));
		index_len = 3;
	} else if(index <= 0x10FFFF) {
		index_as_utf8[0] = 0xF0 + ((index & 0x1C0000) >> 18);
		index_as_utf8[1] = 0x80 + ((index & 0x03F000) >> 12);
		index_as_utf8[2] = 0x80 + ((index & 0x000FC0) >> 6);
		index_as_utf8[3] = 0x80 + ((index & 0x00003F));
		index_len = 4;
	}
}

// Returns -1 and [byte_len] = 0 on failure.
int32_t index_from_utf8(unsigned char &byte_len, const char buf[4])
{
	auto fail = [&byte_len] () {
		byte_len = 0;
		return -1;
	};
	unsigned char codepoint_bits;
	unsigned char bits_seen;
	if((buf[0] & 0x80) == 0) {
		byte_len = 1;
		return buf[0];
	} else if((buf[0] & 0xe0) == 0xc0) {
		codepoint_bits = 11;
		bits_seen = 5;
		byte_len = 2;
	} else if((buf[0] & 0xf0) == 0xe0) {
		codepoint_bits = 16;
		bits_seen = 4;
		byte_len = 3;
	} else if((buf[0] & 0xf8) == 0xf0) {
		codepoint_bits = 21;
		bits_seen = 3;
		byte_len = 4;
	} else {
		return fail();
	}
	int32_t bits = buf[0] & (0xff >> (8 - bits_seen));
	int32_t index = bits << (codepoint_bits - bits_seen);
	unsigned char i = 1;
	for(; i < byte_len; i++) {
		if((buf[i] & 0xc0) != 0x80) {
			return fail();
		}
		bits_seen += 6;
		bits = buf[i] & 0x3f;
		index |= bits << (codepoint_bits - bits_seen);
	}
	if(index >= SURROGATE_START) {
		index -= SURROGATE_COUNT;
	}
	assert(index >= 1 && index <= RENDERED_MAX);
	return index;
}

bool operator ==(const std::string &a, const stringref_t &b)
{
	return (a.size() == b.len) && (a.compare(b.str) == 0);
}

int32_t tlnote_render(const stringref_t &note)
{
	const auto &env = tlnote_env();
	for(int32_t i = 0; i < (int32_t)rendered.size(); i++) {
		if(rendered[i].matches(note, env)) {
			return i;
		}
	}

	tlnote_rendered_t r{ note, env };
	auto index = rendered.size();
	if(index >= RENDERED_MAX) {
		// You absolute madman.
		static int32_t madness_index = 0;
		index = madness_index;
		madness_index = (madness_index + 1) % RENDERED_MAX;
		rendered[index] = r;
	}
	rendered.emplace_back(r);

	// Render any outstanding TL notes that were created
	// while we didn't have an IDirect3DDevice.
	static bool got_unrendered_notes = false;
	if(d3d_ver == D3D_NONE || d3dd == nullptr) {
		got_unrendered_notes = true;
	} else if(got_unrendered_notes) {
		for(auto &v : rendered) {
			v.render(d3d_ver, d3dd);
		}
		got_unrendered_notes = false;
	} else {
		rendered.back().render(d3d_ver, d3dd);
	}
	return index;
}
/// -----------------------------

/// Render calls and detoured functions
/// -----------------------------------
d3dd_EndScene_type *chain_d3dd8_EndScene;
d3dd_EndScene_type *chain_d3dd9_EndScene;

void tlnote_frame(d3d_version_t ver, IDirect3DDevice *d3dd)
{
	d3d_ver = ver;
	::d3dd = d3dd;

	// IDirect3DDevice9::Reset() would fail with D3DERR_INVALIDCALL if any
	// state blocks still exist, so we couldn't just have a single static
	// one that we create once. It's fast enough to do this every frame
	// anyway.
	IDirect3DStateBlock sb_game;
	d3dd_CreateStateBlock(ver, d3dd, D3DSBT_ALL, &sb_game);
	d3dd_CaptureStateBlock(ver, d3dd, sb_game);

	d3dd_ApplyStateBlock(ver, d3dd, sb_game);
	d3dd_DeleteStateBlock(ver, d3dd, sb_game);
}

HRESULT __stdcall tlnote_d3dd8_EndScene(IDirect3DDevice *that)
{
	tlnote_frame(D3D8, that);
	return chain_d3dd8_EndScene(that);
}

HRESULT __stdcall tlnote_d3dd9_EndScene(IDirect3DDevice *that)
{
	tlnote_frame(D3D9, that);
	return chain_d3dd9_EndScene(that);
}
/// -----------------------------------

THCRAP_API void tlnote_show(const tlnote_t tlnote)
{
	if(!tlnote.str) {
		return;
	}
	int32_t index;
	if(tlnote.str->type == TLNOTE_INLINE) {
		index = tlnote_render(tlnote.str->note);
	} else if(tlnote.str->type == TLNOTE_INDEX) {
		unsigned char byte_len;
		index = index_from_utf8(byte_len, tlnote.str->note);
		index -= RENDERED_OFFSET;
		assert(index < (int32_t)rendered.size());
		assert(byte_len >= 1 && byte_len <= 4);
	} else {
		tlnote_log.errorf("Illegal TL note type? (U+%04X)", tlnote.str->type);
		return;
	}
	const auto &tlr = rendered[index];
	// TODO: For now...
	log_printf("(TL note: %.*s)\n", tlr.note.length(), tlr.note.c_str());
}

THCRAP_API tlnote_split_t tlnote_find(stringref_t text, bool inline_only)
{
	const char *p = text.str;
	const char *sepchar_ptr = nullptr;
	for(decltype(text.len) i = 0; i < text.len; i++) {
		if(*p == TLNOTE_INLINE) {
			if(sepchar_ptr) {
				tlnote_log.errorf(
					"Duplicate TL note separator character (U+%04X) in\n\n%s",
					*p, text
				);
				break;
			} else {
				sepchar_ptr = p;
			}
		} else if(*p == TLNOTE_INDEX) {
			auto fail = [text] () -> tlnote_split_t {
				tlnote_log.errorf(
					"U+%04X is reserved for internal TL note handling:\n\n%s",
					TLNOTE_INDEX, text
				);
				return { text };
			};
#define FAIL_IF(cond) \
	if(cond) { \
		return fail(); \
	}
			FAIL_IF(inline_only);
			FAIL_IF(sepchar_ptr);
			FAIL_IF(i + 1 >= text.len);
			unsigned char byte_len;
			auto index = index_from_utf8(byte_len, p + 1);
			FAIL_IF(index < 0);
			auto tlp_len = byte_len + 1;
			auto at_end = text.len - (i + tlp_len);
			FAIL_IF(at_end < 0);
			FAIL_IF(index - RENDERED_OFFSET >= (int)rendered.size());

			tlnote_t tlp = { { p, tlp_len } };
			if(i == 0) {
				return { { p + tlp_len, text.len - tlp_len }, tlp };
			} else if(at_end == 0) {
				return { { text.str, p - text.str }, tlp };
			} else {
				FAIL_IF(1);
			}
#undef FAIL_IF
		}
		p++;
	}
	if(sepchar_ptr) {
		tlnote_t tlnote = { { sepchar_ptr, p - sepchar_ptr } };
		return { { text.str, sepchar_ptr - text.str }, tlnote };
	}
	return { text };
}

THCRAP_API tlnote_encoded_index_t tlnote_prerender(const tlnote_t tlnote)
{
	if(tlnote.str == nullptr || tlnote.str->type != TLNOTE_INLINE) {
		return {};
	}
	stringref_t note = { tlnote.str->note, tlnote.len };
	return { tlnote_render(note) + RENDERED_OFFSET };
}

/// Module functions
/// ----------------
extern "C" __declspec(dllexport) void tlnote_mod_detour(void)
{
	vtable_detour_t d3d8[] = {
		{ 35, tlnote_d3dd8_EndScene, (void**)&chain_d3dd8_EndScene },
	};
	vtable_detour_t d3d9[] = {
		{ 42, tlnote_d3dd9_EndScene, (void**)&chain_d3dd9_EndScene },
	};
	d3d8_device_detour(d3d8, elementsof(d3d8));
	d3d9_device_detour(d3d9, elementsof(d3d9));
}
/// ----------------
