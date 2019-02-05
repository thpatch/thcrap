/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * On-screen display of translation notes.
  */

#pragma once

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

// Searches [text] for a TL note.
THCRAP_API tlnote_split_t tlnote_find(stringref_t text);

// Prerenders the given [tlnote] and returns its new shortened representation
// (prefix byte + UTF-8-encoded internal index), which can then optionally be
// used instead of the full TL note text. Useful for both saving bytes inside
// the strings used to trigger TL notes, and for avoiding lag during regular
// gameplay by deferring the rendering to a less noticeable time.
THCRAP_API tlnote_encoded_index_t tlnote_prerender(const tlnote_t tlnote);
