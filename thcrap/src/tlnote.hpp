/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * On-screen display of translation notes.
  */

#pragma once

/// Rendering values
/// ----------------
// Variables that define the look of rendered TL notes.
class tlnote_env_render_t {
protected:
	// Resolution at which all TL notes will be rendered internally.
	// Region and font size will be interpreted relative to this
	// resolution, and appear scaled down on-screen if the game's
	// actual back buffer is smaller.
	vector2_t _reference_resolution;

	// Width of the on-screen region where TL notes should appear, and
	// therefore the texture they are rendered to.
	float _region_w;

	HFONT _font = nullptr;

public:
	const vector2_t& reference_resolution() const {
		return _reference_resolution;
	}
	const float& region_w() const {
		return _region_w;
	}
	HFONT font() const {
		return _font;
	}

	THCRAP_API bool reference_resolution_set(const vector2_t &newval);
	THCRAP_API bool font_set(const LOGFONTA &lf);
};

struct tlnote_env_t : public tlnote_env_render_t {
	float region_top;
	float region_left;
	float region_h;

	THCRAP_API bool region_size_set(const vector2_t &newval);
};

THCRAP_API tlnote_env_t& tlnote_env();
/// ----------------

// Created by tlnote_prerender().
struct tlnote_encoded_index_t {
private:
	char index_len;
	char type;
	// Large enough for the largest possible UTF-8-encoded codepoint
	char index_as_utf8[4];

public:
	tlnote_encoded_index_t()
		: type('\0') {
	}
	tlnote_encoded_index_t(int32_t index);

	int len() const {
		return (type != '\0') ? (1 + index_len) : 0;
	}

	operator bool() const {
		return type != '\0';
	}
	operator stringref_t() const {
		return { &type, len() };
	}
};

// Can represent both in-line and pre-rendered TL notes.
typedef struct {
	char type;
	char note[1];
} tlnote_string_t;

struct tlnote_t {
	const tlnote_string_t *str;
	// *Not* including the type character.
	int len;

	operator bool() const { return str != nullptr; }

	tlnote_t()
		: str(nullptr), len(0) {
	}
	// Not exported; non-empty instances are only supposed to be
	// created by tlnote_find().
	tlnote_t(const stringref_t str);
};

// Information for splitting a line of text into the regular part and its TL
// note, if there is one.
struct tlnote_split_t {
	// The regular text, not including the TL note. *Never* null-terminated!
	stringref_t regular;

	// Evaluates to false if there is none.
	const tlnote_t tlnote;
};

// Directly shows [tlnote].
THCRAP_API void tlnote_show(const tlnote_t tlnote);

// Searches [text] for a TL note. If [inline_only] is true, the function fails
// if a pre-rendered TL note index is found, so it should always be set when
// running the function on user input.
THCRAP_API tlnote_split_t tlnote_find(stringref_t text, bool inline_only);

// Prerenders the given [tlnote] and returns its new shortened representation
// (prefix byte + UTF-8-encoded internal index), which can then optionally be
// used instead of the full TL note text. Useful for both saving bytes inside
// the strings used to trigger TL notes, and for avoiding lag during regular
// gameplay by deferring the rendering to a less noticeable time.
THCRAP_API tlnote_encoded_index_t tlnote_prerender(const tlnote_t tlnote);
