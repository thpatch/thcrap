/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * On-screen display of translation notes.
  */

#pragma once

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
