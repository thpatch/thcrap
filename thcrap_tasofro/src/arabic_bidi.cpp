/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Arabic bidirectional (bidi) text support.
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"
#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <fribidi.h>

// More information on these characters:
// https://www.w3.org/International/questions/qa-bidi-unicode-controls
static const wchar_t LEFT_TO_RIGHT_OVERRIDE     = L'\u202d';
static const wchar_t POP_DIRECTIONAL_FORMATTING = L'\u202c';
static const wchar_t FIRST_STRONG_ISOLATE       = L'\u2068';
static const wchar_t POP_DIRECTIONAL_ISOLATE    = L'\u2069';

// Returns true if there is more params after this one,
// or false if the last param have been parsed.
static bool copy_params(wchar_t *out, size_t& j, const wchar_t *in, size_t& i, bool add_isolate)
{
    if (add_isolate) {
        out[j] = FIRST_STRONG_ISOLATE; j++;
    }

    while (in[i] && in[i] != L'|' && wcsncmp(in + i, L"}}", 2) != 0) {
        out[j] = in[i];
        i++; j++;
    }

    if (add_isolate) {
        out[j] = POP_DIRECTIONAL_ISOLATE; j++;
    }

    if (in[i] == L'}') {
        out[j] = L'}';
        out[j + 1] = L'}';
        i += 2; j += 2;
        return false;
    }
    else if (in[i] == L'|') {
        out[j] = L'|';
        i++; j++;
        return true;
    }
    else {
        return false;
    }
}

static wchar_t *arabic_escape_tags_for_bidi(const wchar_t *in)
{
	wchar_t *out = new wchar_t[wcslen(in) * 2 + 1]();

    size_t i = 0;
    size_t j = 0;
    while (in[i]) {
        if (wcsncmp(in + i, L"{{", 2) == 0) {
            // Assuming a tag in the form "{{ruby|param1|param2}}", but in a generic way.
            // We want to surround the whole tag with a LEFT-TO-RIGHT OVERRIDE / POP DIRECTIONAL FORMATTING pair,
            // while surrounding each param with a FIRST-STRONG ISOLATE / POP DIRECTIONAL ISOLATE pair.
            out[j] = LEFT_TO_RIGHT_OVERRIDE; j++;

            wcsncpy(out + j, in + i, 2);
            i += 2; j += 2;

            if (copy_params(out, j, in, i, false)) {
               while (copy_params(out, j, in, i, true)) {
               }
            }

            out[j] = POP_DIRECTIONAL_FORMATTING; j++;
        }
        else {
            out[j] = in[i];
            i++; j++;
        }
    }

    return out;
}

// Guaranteed to return a smaller string than its input,
// so we can edit the string in-place instead of allocating a new one.
static void arabic_remove_escapes(wchar_t *str)
{
	size_t j = 0;
	for (size_t i = 0; str[i]; i++) {
		if (str[i] != LEFT_TO_RIGHT_OVERRIDE
		 && str[i] != POP_DIRECTIONAL_FORMATTING
		 && str[i] != FIRST_STRONG_ISOLATE
		 && str[i] != POP_DIRECTIONAL_ISOLATE) {
			str[j] = str[i];
			j++;
		}
	}
	str[j] = '\0';
}

wchar_t *arabic_convert_bidi(const wchar_t *w_in)
{
    wchar_t *w_in_escaped = arabic_escape_tags_for_bidi(w_in);

	size_t len = wcslen(w_in_escaped);
	FriBidiChar *b_in  = new FriBidiChar[len + 1]();
	FriBidiChar *b_out = new FriBidiChar[len + 1]();
	wchar_t *w_out = new wchar_t[len + 1]();

    // UTF-16 to UTF-32 dumb conversion
    std::copy(w_in_escaped, w_in_escaped + len + 1, b_in);

	FriBidiCharType base_dir = FRIBIDI_TYPE_ON;
	fribidi_log2vis(b_in, len, &base_dir, b_out, nullptr, nullptr, nullptr);

    // UTF-32 to UTF-16 dumb conversion
    std::copy(b_out, b_out + len + 1, w_out);
    arabic_remove_escapes(w_out);
	delete[] w_in_escaped;
	delete[] b_in;
	delete[] b_out;
	return w_out;
}

std::string arabic_convert_bidi(const char *in)
{
    WCHAR_T_DEC(in);
    WCHAR_T_CONV(in);

    wchar_t *out = arabic_convert_bidi(in_w);
    WCHAR_T_FREE(in);

    size_t out_s_len = wcslen(out) * UTF8_MUL;
    std::string out_s(out_s_len, '\0');
    out_s_len = StringToUTF8(out_s.data(), out, out_s_len);
    out_s.resize(out_s_len);

    return out_s;
}

std::string arabic_convert_bidi(const std::string& in)
{
    return arabic_convert_bidi(in.c_str());
}
