/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * Unicode conversion functions.
  */

#pragma once

// Maximum length of the largest wchar_t in UTF-8
#define UTF8_MUL 4

/**
  * Converts [str_len] characters of the "narrow" string [str_mb] to UTF-16.
  * Input can either be in UTF-8 or the fallback codepage specified by a call
  * to w32u8_set_fallback_codepage().
  *
  * [str_len] can be -1 - this assumes that [str_mb] is null-terminated and
  * calculates the length automatically.
  *
  * If [str_w] and [str_len] are NULL/0, the function returns the required
  * length, in wide characters, for a buffer that fits [str_mb].
  */
int StringToUTF16(wchar_t *str_w, const char *str_mb, int str_len);

// StringToUTF16 for VLAs. Returs NULL if [str_mb] is NULL,
// and frees the VLA if necessary.
wchar_t* StringToUTF16_VLA(wchar_t *str_w, const char *str_mb, int str_len);

// Converts a UTF-16 string to UTF-8
int StringToUTF8(char *str_utf8, const wchar_t *str_w, int str_len);

// Returns [str] in UTF-8.
// Return value has to be free()d by the caller!
char* EnsureUTF8(const char *str, int str_len);
