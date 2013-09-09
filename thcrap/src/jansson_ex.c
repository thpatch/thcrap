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
	const char *str = json_string_value(val);
	if(str) {
		int base = 10;
		size_t offset = 0;
		size_t ret = 0;

		if(strlen(str) > 2) {
			// Module-relative hex values
			if(!strnicmp(str, "Rx", 2)) {
				ret += (size_t)GetModuleHandle(NULL);
				base = 16;
				offset = 2;
			} else if(!strnicmp(str, "0x", 2)) {
				base = 16;
				offset = 2;
			}
		}
		ret += strtol(str + offset, NULL, base);
		return ret;
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
	json_t *val = json_array_get(arr, ind);
	if(val) {
		size_t ret = json_hex_value(val);
		if(json_is_string(val)) {
			// Rewrite the JSON value
			json_array_set_new(arr, ind, json_integer(ret));
		}
		return ret;
	}
	return 0;
}

const char* json_array_get_string(const json_t *arr, const size_t ind)
{
	return json_string_value(json_array_get(arr, ind));
}

const char* json_array_get_string_safe(const json_t *arr, const size_t ind)
{
	const char *ret = json_array_get_string(arr, ind);
	if(!ret) {
		ret = "";
	}
	return ret;
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

json_t* json_object_numkey_get(json_t *object, const json_int_t key)
{
	char key_str[64];
	snprintf(key_str, 64, "%lld", key);
	return json_object_get(object, key_str);
}

json_t* json_object_hexkey_get(json_t *object, const size_t key)
{
#define addr_key_len 2 + (sizeof(void*) * 2) + 1
	char key_str[addr_key_len];
	json_t *ret = NULL;

	snprintf(key_str, addr_key_len, "Rx%x", key - (size_t)GetModuleHandle(NULL));
	ret = json_object_get(object, key_str);
	if(!ret) {
		snprintf(key_str, addr_key_len, "0x%x", key);
		ret = json_object_get(object, key_str);
	}
	return ret;
}

size_t json_object_get_hex(json_t *object, const char *key)
{
	json_t *val = json_object_get(object, key);
	if(val) {
		size_t ret = json_hex_value(val);
		if(json_is_string(val)) {
			 // Rewrite the JSON value
			json_object_set_new_nocheck(object, key, json_integer(ret));
		}
		return ret;
	}
	return 0;
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

int json_object_merge(json_t *old_obj, json_t *new_obj)
{
	const char *key;
	json_t *new_val;
	
	if(!old_obj || !new_obj) {
		return -1;
	}
	json_object_foreach(new_obj, key, new_val) {
		json_t *old_val = json_object_get(old_obj, key);
		if(json_is_object(old_val)) {
			// Recursion!
			json_object_merge(old_val, new_val);
		} else {
			json_object_set_nocheck(old_obj, key, new_val);
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
