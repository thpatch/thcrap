/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * On-screen display of translation notes.
  */

#include "thcrap.h"
#include "tlnote.hpp"

/// Error reporting and debugging
/// -----------------------------
logger_t tlnote_log("TL note error");
/// -----------------------------

/// Structures
/// ----------
tlnote_t::tlnote_t(const stringref_t str)
	: str((tlnote_string_t *)str.str), len(str.len - 1) {
	assert(str.str[0] == TLNOTE_INLINE);
	assert(str.len > 1);
}
/// ----------

THCRAP_API void tlnote_show(const tlnote_t tlnote)
{
	if(!tlnote.str) {
		return;
	}
	if(tlnote.str->type != TLNOTE_INLINE) {
		return;
	}
	// TODO: For now...
	log_printf("(TL note: %.*s)\n", tlnote.len, tlnote.str->note);
}

THCRAP_API tlnote_split_t tlnote_find(stringref_t text)
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
		}
		p++;
	}
	if(sepchar_ptr) {
		tlnote_t tlnote = { { sepchar_ptr, p - sepchar_ptr } };
		return { sepchar_ptr - text.str, tlnote };
	}
	return { text.len };
}

