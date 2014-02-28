/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Additional JSON-related functions.
  */

#pragma once

// Returns [json] if the object is still alive, and NULL if it was deleted.
json_t* json_decref_safe(json_t *json);

/**
  * Unfortunately, JSON doesn't support native hexadecimal values.
  * This function works with both string and integer values and returns the
  * correct, positive number at the machine's word size.
  * The following string prefixes are supported:
  *
  *	- "0x": Hexadecimal, as expected.
  *	- "Rx": Hexadecimal value relative to the base address of the main module
  *	        of the current process.
  *	- Everything else is parsed as a decimal number.
  */
size_t json_hex_value(json_t *val);

/// Arrays
/// ------
// Like json_array_set(_new), but expands the array if necessary.
int json_array_set_expand(json_t *arr, size_t ind, json_t *value);
int json_array_set_new_expand(json_t *arr, size_t ind, json_t *value);

// Get the integer value of [ind] in [array],
// automatically converting the JSON value to an integer if necessary.
size_t json_array_get_hex(json_t *arr, const size_t ind);

// Convenience function for json_string_value(json_array_get(object, ind));
const char* json_array_get_string(const json_t *arr, const size_t ind);

// Same as json_array_get_string(), but returns an empty string ("")
// if element #[ind] in [arr] is no valid string.
const char* json_array_get_string_safe(const json_t *arr, const size_t ind);

// Convert [argc] UTF-16 strings from [wargv] to a JSON array containing
// UTF-8 strings. Useful for dealing with command-line parameters on Windows.
json_t *json_array_from_wchar_array(int argc, const wchar_t *wargv[]);

// "Flexible array" size - returns the array size if [json] is an array,
// 1 if it's any other valid JSON object, and 0 if it's NULL.
size_t json_flex_array_size(const json_t *json);

// Returns the [ind]th element of [flarr] if it's an array,
// or [flarr] itself otherwise.
json_t *json_flex_array_get(json_t *flarr, size_t ind);

#define json_flex_array_foreach(flarr, ind, val) \
	for(ind = 0; 	(\
			ind < json_flex_array_size(flarr) && \
			(val = json_flex_array_get(flarr, ind)) \
		); \
		ind++)
/// ------

/// Objects
/// -------
// Same as json_object_get, but creates a new JSON value of type [type]
// if the [key] doesn't exist.
json_t* json_object_get_create(json_t *object, const char *key, json_type type);

// json_object_get for numeric keys
json_t* json_object_numkey_get(const json_t *object, const json_int_t key);

// json_object_get for hexadecimal keys.
// These *must* have the format "0x%x" or "Rx%x" (for values relative to the
// base address, see json_hex_value()). Padding %x with zeroes will *not* work.
json_t* json_object_hexkey_get(const json_t *object, const size_t key);

// Get the integer value of [key] in [object], automatically
// converting the JSON value to an integer if necessary.
size_t json_object_get_hex(json_t *object, const char *key);

// Convenience function for json_string_value(json_object_get(object, key));
const char* json_object_get_string(const json_t *object, const char *key);

// Merge [new_obj] recursively into [old_obj].
// [new_obj] has priority; any element of [old_obj] that is present
// in [new_obj] and is *not* an object itself is overwritten.
// Returns [old_obj].
json_t* json_object_merge(json_t *old_obj, const json_t *new_obj);

// Return an alphabetically sorted JSON array of the keys in [object].
json_t* json_object_get_keys_sorted(const json_t *object);
/// -------

// Wrapper around json_loadb and json_load_file with indirect UTF-8 filename
// support and nice error reporting.
// [source] can be specified to show the source of the JSON buffer in case of an error
// (since json_error_t::source would just say "<buffer>").
json_t* json_loadb_report(const void *buffer, size_t buflen, size_t flags, const char *source);
json_t* json_load_file_report(const char *json_fn);

// log_print for json_dump
int json_dump_log(const json_t *json, size_t flags);
