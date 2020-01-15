/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Persistent string storage, and translation of hardcoded strings.
  */

#pragma once

/// Translation
/// -----------
// Returns the ID for the given [str], or NULL if it couldn't be determined.
// Currently only looking up the pointer value in the stringlocs map.
const char* strings_id(const char* str);

// Returns [id] from the string definition table,
// or NULL if no string for [id] is available.
const json_t* strings_get(const char *id);

#ifdef __cplusplus
}

// Like strings_get(), but with a fallback onto [def] if no string for [id]
// is available.
struct string_named_t {
	const char *id;
	const stringref_t &fallback;
};

stringref_t strings_get_fallback(const string_named_t& sn);

extern "C" {
#endif

// Returns the translated string for [in] from the string definition table,
// or [in] itself if no translation is available.
// Optionally returns the length of the returned string in [out_len], without
// the terminating \0, if [out_len] is not a nullptr.
const char* __cdecl strings_lookup(const char *in, size_t *out_len);

// String lookup for variable argument lists. Parses [format] and calls
// [strings_lookup] for every string parameter in [va].
void strings_va_lookup(va_list va, const char *format);
/// -----------

/// Persistent storage
/// ------------------
// Returns a pointer to the string in the persistent storage slot [slot], or
// NULL on allocation failure. The string's buffer is automatically resized
// to contain at least [min_len] bytes.
char* strings_storage_get(const size_t slot, size_t min_len);

// Safe and persistent sprintf handler.
// This function should be inserted via binary hacks everywhere the patched
// program calls sprintf, as it guarantees a sufficiently large buffer for the
// result. [format] and [va] are the respective parameters of vsprintf. The
// storage [slot] can be any value, but calling this function again with the
// same [slot] deletes the result from the previous call.
// Returns a pointer to the resulting string.
const char* strings_vsprintf(const size_t slot, const char *format, va_list va);
const char* strings_sprintf(const size_t slot, const char *format, ...);

const char* strings_vsprintf_msvcrt14(const char *format, const size_t slot, va_list va);

// Clears the string in [slot].
const char* strings_strclr(const size_t slot);

// Concatenates [src] to the existing string in [slot].
const char* strings_strcat(const size_t slot, const char *src);

// Replaces all occurences of [src] in [slot] with [dst]. If [dst] is NULL,
// the function defaults it to an empty string, thus removing all occurences
// of [src]. Currently, this function doesn't do translation lookup since we
// don't yet have any specific use cases.
const char* strings_replace(const size_t slot, const char *src, const char *dst);
/// ------------------

void strings_mod_init(void);
// Adds string lookup wrappers to functions that don't have them yet.
void strings_mod_detour(void);
void strings_mod_repatch(json_t *files_changed);
void strings_mod_exit(void);
