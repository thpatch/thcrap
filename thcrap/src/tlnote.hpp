/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * On-screen display of translation notes.
  */

#pragma once

// UTF-8-encoded codepoint that separates in-line TL note text (after) from
// regular text (before).
#define TLNOTE_INLINE 0x14

// Information for splitting a line of text into the regular part and its TL
// note, if there is one.
struct tlnote_split_t {
	// Length of the regular text before the TL note.
	// Equal to the length of the input string if there is none.
	int regular_len;

	// TL note making up the rest of the line, or a nullptr if there is none.
	// (Not a stringref_t because of that!)
	const char *tlnote;
	int tlnote_len;
};

// Directly shows [tlnote].
THCRAP_API void tlnote_show(const stringref_t tlnote);

// Searches [text] for a TL note.
THCRAP_API tlnote_split_t tlnote_find(stringref_t text);
