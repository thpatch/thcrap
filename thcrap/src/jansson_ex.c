/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Additional JSON-related functions.
  */

#include "thcrap.h"

size_t json_hex_value(json_t *val)
{
	const char *str;

	if(!val) {
		return 0;
	}
	str = json_string_value(val);
	if(str) {
		return strtol(str, NULL, str_num_base(str));
	}
	return (size_t)json_integer_value(val);
}

wchar_t* json_string_value_utf16(const json_t *str_object)
{
	size_t str_len;
	const char *str_utf8;
	wchar_t *str_utf16 = NULL;

	if(!json_is_string(str_object)) {
		return NULL;
	}
	str_utf8 = json_string_value(str_object);
	str_len = strlen(str_utf8) + 1;
	str_utf16 = (wchar_t*)malloc(str_len * sizeof(wchar_t));

	StringToUTF16(str_utf16, str_utf8, str_len);
	return str_utf16;
}

int json_array_set_expand(json_t *arr, size_t ind, json_t *value)
{
	size_t arr_size = json_array_size(arr);
	if(ind >= arr_size) {
		int ret;
		size_t i;
		for(i = arr_size; i <= ind; i++) {
			ret = json_array_append(arr, value);
		}
		return ret;
	} else {
		return json_array_set(arr, ind, value);
	}
}

size_t json_array_get_hex(json_t *arr, const size_t ind)
{
	const char *str;
	size_t ret;

	json_t *value = json_array_get(arr, ind);
	if(!value) {
		return 0;
	}
	str = json_string_value(value);
	if(str) {
		// Convert the string value to integer and rewrite the JSON value
		ret = strtol(str, NULL, str_num_base(str));
		json_array_set_new(arr, ind, json_integer(ret));
	} else {
		ret = (size_t)json_integer_value(value);
	}
	return ret;
}

const char* json_array_get_string(const json_t *arr, const size_t ind)
{
	return json_string_value(json_array_get(arr, ind));
}

wchar_t* json_array_get_string_utf16(const json_t *arr, const size_t ind)
{
	return json_string_value_utf16(json_array_get(arr, ind));
}

json_t* json_object_get_create(json_t *object, const char *key, json_t *new_object)
{
	json_t *ret = json_object_get(object, key);
	if(!ret) {
		json_object_set_new(object, key, new_object);
		return new_object;
	} else {
		json_decref(new_object);
	}
	return ret;
}

size_t json_object_get_hex(json_t *object, const char *key)
{
	const char *str;
	size_t ret;

	json_t *value = json_object_get(object, key);
	if(!value) {
		return 0;
	}
	str = json_string_value(value);
	if(str) {
		// Convert the string value to integer and rewrite the JSON value
		ret = strtol(str, NULL, str_num_base(str));
		json_object_set_new_nocheck(object, key, json_integer(ret));
	} else {
		ret = (size_t)json_integer_value(value);
	}
	return ret;
}

const char* json_object_get_string(const json_t *object, const char *key)
{
	if(!key) {
		return NULL;
	}
	return json_string_value(json_object_get(object, key));
}

wchar_t* json_object_get_string_utf16(const json_t *object, const char *key)
{
	if(!key) {
		return NULL;
	}
	return json_string_value_utf16(json_object_get(object, key));
}

int json_object_merge(json_t *dest, json_t *src)
{
	const char *src_key;
	json_t *src_obj;
	
	if(!dest || !src) {
		return -1;
	}
	json_object_foreach(src, src_key, src_obj) {
		json_t *dest_obj = json_object_get(dest, src_key);
		if(dest_obj) {
			if(json_is_object(dest_obj)) {
				// Recursion!
				json_object_merge(dest_obj, src_obj);
			/**
			  * Yes, arrays should be completely overwritten, too.
			  * Any objections?
			  */
			/*
			} else if(json_is_array(src_obj) && json_is_array(dest_obj)) {
				json_array_extend(dest_obj, json_deep_copy(src_obj));
			}*/
			} else {
				json_object_set_new_nocheck(dest, src_key, json_deep_copy(src_obj));
			}
		}
		else {
			json_object_set_new_nocheck(dest, src_key, json_deep_copy(src_obj));
		}
	}
	
	return 0;
}

static int __cdecl object_key_compare_keys(const void *key1, const void *key2)
{
    return strcmp(*(const char **)key1, *(const char **)key2);
}

json_t* json_object_get_keys_sorted(const json_t *object)
{
	if(!object) {
		return NULL;
	}
	{
		json_t *ret;
		size_t size = json_object_size(object);
		VLA(const char*, keys, size);
		size_t i;
		void *iter = json_object_iter((json_t *)object);

		if(!keys) {
			return NULL;
		}

		i = 0;
		while(iter) {
			keys[i] = json_object_iter_key(iter);
			iter = json_object_iter_next((json_t *)object, iter);
			i++;
		}

		qsort(keys, size, sizeof(const char *), object_key_compare_keys);

		ret = json_array();
		for(i = 0; i < size; i++) {
			json_array_append_new(ret, json_string(keys[i]));
		}
		VLA_FREE(keys);
		return ret;
	}
}

json_t* json_loadb_report(const void *buffer, size_t buflen, size_t flags, const char *source)
{
	json_error_t error;
	json_t *ret;

	if(!buffer) {
		return NULL;
	}
	ret = json_loadb((char*)buffer, buflen, JSON_DISABLE_EOF_CHECK, &error);
	if(!ret) {
		log_mboxf(NULL, MB_OK | MB_ICONSTOP,
			"JSON parsing error:\n"
			"\n"
			"\t%s\n"
			"\n"
			"(%s%sline %d, column %d)",
			error.text, source ? source : "", source ? ", " : "", error.line, error.column
		);
	}
	return ret;
}

json_t* json_load_file_report(const char *json_fn)
{
	size_t json_size;
	void *json_buffer;
	BYTE *json_p;
	json_t *json = NULL;
	const unsigned char utf8_bom[] = {0xef, 0xbb, 0xbf};
	
	json_p = json_buffer = file_read(json_fn, &json_size);
	if(!json_buffer || !json_size) {
		return NULL;
	}
	// Skip UTF-8 byte order mark, if there
	if(!memcmp(json_p, utf8_bom, sizeof(utf8_bom))) {
		json_p += sizeof(utf8_bom);
		json_size -= sizeof(utf8_bom);
	}
	json = json_loadb_report(json_p, json_size, JSON_DISABLE_EOF_CHECK, json_fn);
	SAFE_FREE(json_buffer);
	return json;
}

static int __cdecl dump_to_log(const char *buffer, size_t size, void *data)
{
	log_nprint(buffer, size);
	return 0;
}

int json_dump_log(const json_t *json, size_t flags)
{
	return json_dump_callback(json, dump_to_log, NULL, flags);
}
