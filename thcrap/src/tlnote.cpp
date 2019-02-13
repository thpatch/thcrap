/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * On-screen display of translation notes.
  */

#include "thcrap.h"
#include <algorithm>
#include <vector>
#include "minid3d.h"
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

/// Indexing of rendered TL notes
/// -----------------------------
std::vector<std::string> rendered;

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
	auto elm = std::find(rendered.begin(), rendered.end(), note);
	if(elm != rendered.end()) {
		return (int32_t)(elm - rendered.begin());
	}
	std::string note_owned{ note.str, (size_t)note.len };
	auto index = rendered.size();
	if(index >= RENDERED_MAX) {
		// You absolute madman.
		static int32_t madness_index = 0;
		index = madness_index;
		madness_index = (madness_index + 1) % RENDERED_MAX;
		rendered[index] = note_owned;
	}
	rendered.push_back(note_owned);
	return index;
}
/// -----------------------------

/// Render calls and detoured functions
/// -----------------------------------
d3dd_EndScene_type *chain_d3dd8_EndScene;
d3dd_EndScene_type *chain_d3dd9_EndScene;

void tlnote_frame(d3d_version_t ver, IDirect3DDevice *d3dd)
{
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
	const auto &note = rendered[index];
	// TODO: For now...
	log_printf("(TL note: %.*s)\n", note.length(), note.c_str());
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
