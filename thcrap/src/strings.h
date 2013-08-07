/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Translation of hardcoded strings.
  */

#pragma once

#define HAVE_STRINGS 1

// Returns the translated string for [in] from the string definition table,
// or [in] itself if no translation is available.
// Optionally returns the length of the returned string in [out_len],
// if not NULL.
const char* strings_lookup(const char *in, size_t *out_len);

void strings_init();
void strings_exit();
