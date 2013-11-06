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

// Returns [id] from the string definition table,
// or NULL if no string for [id] available.
const char* strings_get(const char *id);

// Returns the translated string for [in] from the string definition table,
// or [in] itself if no translation is available.
// Optionally returns the length of the returned string in [out_len],
// if not NULL.
const char* strings_lookup(const char *in, size_t *out_len);

// String lookup for variable argument lists. Parses [format] and calls
// [strings_lookup] for every string parameter in [va].
void strings_va_lookup(va_list va, const char *format);

/// Persistent storage
/// ------------------
// Returns a pointer to the string in the persistent storage slot [slot], or
// NULL on allocation failure. The string's buffer is automatically resized
// to contain at least [min_len] bytes.
char* strings_storage_get(const size_t slot, size_t min_len);

// Safe and persistent sprintf handler.
// This function should be inserted via binary hacks everywhere a game
// calls sprintf, as it guarantees a sufficiently large buffer for the result.
// [format] and [va] are the respective parameters of vsprintf, [addr] is the
// storage slot. This can be any value, but calling this function again with
// the same [addr] deletes the result from the previous call.
// Returns a pointer to the resulting string.
const char* strings_vsprintf(const size_t addr, const char *format, va_list va);
const char* strings_sprintf(const size_t addr, const char *format, ...);

// Clears the string in [slot].
const char* strings_strclr(const size_t slot);

// Concatenates [src] to the existing string in [slot].
const char* strings_strcat(const size_t slot, const char *src);
/// ------------------

void strings_init(void);
// Adds string lookup wrappers to functions that don't have them yet.
int strings_detour(HMODULE hMod);
void strings_exit(void);
