/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Additional JSON-related functions.
  */

#pragma once

// Expands to an integer constant expression evaluating to a close upper bound
// on the number the number of decimal digits in a value expressible in the
// integer type given by the argument (if it is a type name) or the integer
// type of the argument (if it is an expression). The meaning of the resulting
// expression is unspecified for other arguments.
// https://stackoverflow.com/questions/43787672/the-max-number-of-digits-in-an-int-based-on-number-of-bits
// Useful to calculate buffer sizes for itoa() calls.
#define DECIMAL_DIGITS_BOUND(t) (241 * sizeof(t) / 100 + 1)

// Returns [json] if the object is still alive, and NULL if it was deleted.
json_t* json_decref_safe(json_t *json);

/**
  * Unfortunately, JSON doesn't support native hexadecimal values.
  * This function works with both JSON integers and strings, parsing the
  * latter using str_address_value().
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

// "Flexible array" size - returns the array size if [json] is an array,
// 1 if it's any other valid JSON object, and 0 if it's NULL.
size_t json_flex_array_size(const json_t *json);

// Returns the [ind]th element of [flarr] if it's an array,
// or [flarr] itself otherwise.
json_t *json_flex_array_get(json_t *flarr, size_t ind);

const char* json_flex_array_get_string_safe(json_t *flarr, size_t ind);

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

// json_object_get for numeric keys in decimal
json_t* json_object_numkey_get(const json_t *object, const json_int_t key);

// Get the integer value of [key] in [object], automatically
// converting the JSON value to an integer if necessary.
size_t json_object_get_hex(json_t *object, const char *key);

// Convenience function for json_string_value(json_object_get(object, key));
const char* json_object_get_string(const json_t *object, const char *key);

// Merge [new_obj] recursively into [old_obj].
// [new_obj] has priority; any element of [old_obj] that is present
// in [new_obj] and is *not* an object itself is overwritten.
// Returns [old_obj] if both are JSON objects, or a new reference to
// [new_obj] otherwise.
json_t* json_object_merge(json_t *old_obj, json_t *new_obj);

// Return an alphabetically sorted JSON array of the keys in [object].
json_t* json_object_get_keys_sorted(const json_t *object);
/// -------

/// "Custom types"
/// --------------
#ifdef __cplusplus
}

struct json_custom_value_t {
	std::string err;
};

struct json_vector2_t : public json_custom_value_t {
	vector2_t v;
};

// Parses a [X, Y] JSON array, returning an error if [arr] doesn't match
// this format. Currently only accepting positive integers!
json_vector2_t json_vector2_value(const json_t *arr);

struct json_xywh_t : public json_custom_value_t {
	xywh_t v;
};

// Parses a [X, Y, width, height] JSON array, returning an error if [arr]
// doesn't match this format. Currently only accepting positive integers!
json_xywh_t json_xywh_value(const json_t *arr);

extern "C" {
#endif
/// --------------

// Wrapper around json_load_file with indirect UTF-8 filename
// support and nice error reporting.
json_t* json_load_file_report(const char *json_fn);

// log_print for json_dump
int json_dump_log(const json_t *json, size_t flags);
