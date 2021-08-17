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
// TODO: The new JSON5 API automatically converts raw hex values to integers
// for use with jansson, thus rendering this function obsolete. If the values
// must remain as strings, use json_eval_int instead.
size_t json_hex_value(json_t *val);

/// Strings
/// ------
TH_CALLER_FREE char* json_string_copy(const json_t *object);

/// Arrays
/// ------
// Like json_array_set(_new), but expands the array if necessary.
int json_array_set_expand(json_t *arr, size_t ind, json_t *value);
int json_array_set_new_expand(json_t *arr, size_t ind, json_t *value);

// Get the integer value of [ind] in [array],
// automatically converting the JSON value to an integer if necessary.
size_t json_array_get_hex(json_t *arr, const size_t ind);

TH_CALLER_FREE char** json_string_array_copy(const json_t *arr);

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

#define json_array_foreach_scoped(ind_type, ind, arr, val) \
	for(ind_type ind = 0, ind ## _max = json_array_size(arr); \
		ind < ind ## _max ? (val = json_array_get(arr, ind)), 1 : 0; \
		ind++)

#define json_flex_array_foreach(flarr, ind, val) \
	for(ind = 0; 	(\
			ind < json_flex_array_size(flarr) && \
			(val = json_flex_array_get(flarr, ind)) \
		); \
		ind++)

#define json_flex_array_foreach_scoped(ind_type, ind, flarr, val) \
	for(ind_type ind = 0, ind ## _max = (ind_type)json_flex_array_size(flarr); \
			ind < ind ## _max ? (val = json_flex_array_get(flarr, ind)), 1 : 0 ; \
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

// Convenience function for json_string_array_copy(json_object_get(object, key));
TH_CALLER_FREE char** json_object_get_string_array_copy(const json_t *object, const char* key);

// Convenience function for json_string_value(json_object_get(object, key));
const char* json_object_get_string(const json_t *object, const char *key);

// Convenience function for json_string_copy(json_object_get(object, key));
TH_CALLER_FREE char* json_object_get_string_copy(const json_t *object, const char *key);

// Merge [new_obj] recursively into [old_obj].
// [new_obj] has priority; any element of [old_obj] that is present
// in [new_obj] and is *not* an object itself is overwritten.
// Returns [old_obj] and decrements [new_obj] if no errors occur, or
// [new_obj] otherwise.
json_t* json_object_merge(json_t *old_obj, json_t *new_obj);

// Return an alphabetically sorted JSON array of the keys in [object].
json_t* json_object_get_keys_sorted(const json_t *object);
/// -------

/// "Custom types"
/// --------------
#ifdef __cplusplus
extern "C++" {

struct DerefAutoJson {
	inline void operator()(json_t* obj) {
		json_decref(obj);
	}
};

using UniqueJsonPtr = std::unique_ptr<json_t, DerefAutoJson>;

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
THCRAP_API json_xywh_t json_xywh_value(const json_t *arr);

}
#endif
/// --------------

// Load a json file with the json5 syntax.
json_t *json5_loadb(const void *buffer, size_t size, char **error);

// Wrapper around json_load_file and json5_loadb with
// indirect UTF-8 filename support and nice error reporting.
json_t* json_load_file_report(const char *json_fn);

// log_print for json_dump
void json_dump_log(const json_t *json, size_t flags);

/// ------

/// Evaluation
/// -------

size_t json_string_expression_value(json_t* json);

typedef enum {
	JEVAL_SUCCESS = 0,

	// Json object was null
	JEVAL_NULL_PTR = 1, 

	// Strict flag was specified and the json
	// could not be converted to the output type
	JEVAL_ERROR_STRICT_TYPE_MISMATCH = 2,

	// NoExprs flag was specified and the json
	// contained a string expression
	JEVAL_ERROR_STRING_NO_EXPRS = 3,

	// Int64RangeErr flag was specified and the
	// json integer value would not fit in size_t
	JEVAL_OUT_OF_RANGE = 4 
} jeval_error_t;

enum {
	// Default flags: JEVAL_LENIENT | JEVAL_USE_EXPRS | JEVAL_INT64_TRUNCATE
	JEVAL_DEFAULT		= 0b00000,

	// Json evaluation will attempt to provide
	// a return value regardless of json type
	JEVAL_LENIENT		= 0b00000,

	// Json evaluation will return an error code
	// when parsing a json type incompatible with
	// the return type
	JEVAL_STRICT		= 0b00100,

	// Json evaluation will consider string types
	// to be valid for returning a numeric value
	// and parse them as expressions
	JEVAL_USE_EXPRS		= 0b00000,

	// Json evaluation will return an error code
	// when parsing a json string type
	JEVAL_NO_EXPRS		= 0b01000,

	// Json evaluation will truncate large values
	// when returning a 32-bit integer.
	JEVAL_INT_TRUNCATE	= 0b00000,

	// Json evaluation will return an error code
	// instead of truncating large values when
	// returning a 32-bit integer.
	JEVAL_INT_RANGE_ERR	= 0b10000
};
typedef uint8_t jeval_flags_t;

// Evaluate the JSON [val] according to the supplied [flags] and
// store the result in [out], returning a json_eval_error_t
// indicating whether the operation was successful. [out] is not
// modified for any return value except JEVAL_SUCCESS.
TH_CHECK_RET jeval_error_t json_eval_bool(json_t* val, bool* out, jeval_flags_t flags);
TH_CHECK_RET jeval_error_t json_eval_int(json_t* val, size_t* out, jeval_flags_t flags);
TH_CHECK_RET jeval_error_t json_eval_int64(json_t* val, json_int_t* out, jeval_flags_t flags);
TH_CHECK_RET jeval_error_t json_eval_real(json_t* val, double* out, jeval_flags_t flags);
TH_CHECK_RET jeval_error_t json_eval_number(json_t* val, double* out, jeval_flags_t flags);

// Convenience functions for json_eval_type(json_object_get(object, key), out, flags);
TH_CHECK_RET jeval_error_t json_object_get_eval_bool(json_t* object, const char* key, bool* out, jeval_flags_t flags);
TH_CHECK_RET jeval_error_t json_object_get_eval_int(json_t* object, const char* key, size_t* out, jeval_flags_t flags);
TH_CHECK_RET jeval_error_t json_object_get_eval_int64(json_t* object, const char* key, json_int_t* out, jeval_flags_t flags);
TH_CHECK_RET jeval_error_t json_object_get_eval_real(json_t* object, const char* key, double* out, jeval_flags_t flags);
TH_CHECK_RET jeval_error_t json_object_get_eval_number(json_t* object, const char* key, double* out, jeval_flags_t flags);

// Evaluate the JSON [val] according to the supplied [flags] and
// returning either the result or [default_ret] if the operation
// could not be performed.
bool json_eval_bool_default(json_t* val, bool default_ret, jeval_flags_t flags);
size_t json_eval_int_default(json_t* val, size_t default_ret, jeval_flags_t flags);
json_int_t json_eval_int64_default(json_t* val, json_int_t default_ret, jeval_flags_t flags);
double json_eval_real_default(json_t* val, double default_ret, jeval_flags_t flags);
double json_eval_number_default(json_t* val, double default_ret, jeval_flags_t flags);

// Convenience functions for json_eval_type_default(json_object_get(object, key), default_ret, flags);
bool json_object_get_eval_bool_default(json_t* object, const char* key, bool default_ret, jeval_flags_t flags);
size_t json_object_get_eval_int_default(json_t* object, const char* key, size_t default_ret, jeval_flags_t flags);
json_int_t json_object_get_eval_int64_default(json_t* object, const char* key, json_int_t default_ret, jeval_flags_t flags);
double json_object_get_eval_real_default(json_t* object, const char* key, double default_ret, jeval_flags_t flags);
double json_object_get_eval_number_default(json_t* object, const char* key, double default_ret, jeval_flags_t flags);
