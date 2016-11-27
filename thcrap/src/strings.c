/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Persistent string storage, and translation of hardcoded strings.
  */

#include "thcrap.h"
#include <assert.h>

/// Detour chains
/// -------------
W32U8_DETOUR_CHAIN_DEF(MessageBox);
/// -------------

// Length-prefixed string object used for persistent storage
typedef struct {
	size_t len;
	char str;
} storage_string_t;

static json_t *strings_storage = NULL;

#define addr_key_len 2 + (sizeof(void*) * 2) + 1

const json_t* strings_get(const char *id)
{
	return json_object_get(jsondata_get("stringdefs.js"), id);
}

const char* strings_lookup(const char *in, size_t *out_len)
{
	const json_t *stringlocs = NULL;
	const char *id_key = NULL;
	const char *ret = in;

	if(!in) {
		return in;
	}

	stringlocs = jsondata_game_get("stringlocs.js");
	id_key = json_string_value(json_object_hexkey_get(stringlocs, (size_t)in));

	if(id_key) {
		const char *new_str = json_string_value(strings_get(id_key));
		if(new_str && new_str[0]) {
			ret = new_str;
		}
	}
	if(out_len) {
		*out_len = strlen(ret) + 1;
	}
	return ret;
}

void strings_va_lookup(va_list va, const char *format)
{
	const char *p = format;
	while(*p) {
		printf_format_t fmt;
		int i;

		// Skip characters before '%'
		for(; *p && *p != '%'; p++);
		if(!*p) {
			break;
		}
		// *p == '%' here
		p++;

		// output a single '%' character
		if(*p == '%') {
			p++;
			continue;
		}
		p = printf_format_parse(&fmt, p);
		for(i = 0; i < fmt.argc_before_type; i++) {
			va_arg(va, int);
		}
		if(fmt.type == 's' || fmt.type == 'S') {
			*(const char**)va = strings_lookup(*(const char**)va, NULL);
		}
		for(i = 0; i < fmt.type_size_in_ints; i++) {
			va_arg(va, int);
		}
	}
}

char* strings_storage_get(const size_t slot, size_t min_len)
{
	storage_string_t *ret = NULL;
	char addr_key[addr_key_len];

	itoa(slot, addr_key, 16);
	ret = (storage_string_t*)json_object_get_hex(strings_storage, addr_key);

	// MSVCRT's realloc implementation moves the buffer every time, even if the
	// new length is shorter...
	if(!ret || (min_len && ret->len < min_len)) {
		storage_string_t *ret_new = (storage_string_t*)realloc(ret, min_len + sizeof(storage_string_t));
		// Yes, this correctly handles a realloc failure.
		if(ret_new) {
			ret_new->len = min_len;
			if(!ret) {
				ret_new->str = 0;
			}
			json_object_set_new(strings_storage, addr_key, json_integer((size_t)ret_new));
			ret = ret_new;
		}
	}
	if(ret) {
		return &ret->str;
	}
	return NULL;
}

const char* strings_vsprintf(const size_t slot, const char *format, va_list va)
{
	char *ret = NULL;
	size_t str_len;

	format = strings_lookup(format, NULL);
	strings_va_lookup(va, format);

	if(!format) {
		return NULL;
	}
	str_len = _vscprintf(format, va) + 1;

	ret = strings_storage_get(slot, str_len);
	if(ret) {
		vsprintf(ret, format, va);
		return ret;
	}
	// Try to save the situation at least somewhat...
	return format;
}

const char* strings_sprintf(const size_t slot, const char *format, ...)
{
	va_list va;
	const char *ret;
	va_start(va, format);
	ret = strings_vsprintf(slot, format, va);
	return ret;
}

const char* strings_strclr(const size_t slot)
{
	char *ret = strings_storage_get(slot, 0);
	if(ret) {
		ret[0] = 0;
	}
	return ret;
}

const char* strings_strcat(const size_t slot, const char *src)
{
	char *ret = strings_storage_get(slot, 0);
	size_t ret_len = strlen(ret);
	size_t src_len;

	src = strings_lookup(src, &src_len);

	ret = strings_storage_get(slot, ret_len + src_len);
	if(ret) {
		strncpy(ret + ret_len, src, src_len);
		return ret;
	}
	// Try to save the situation at least somewhat...
	return src;
}

const char* strings_replace(const size_t slot, const char *src, const char *dst)
{
	char *ret = strings_storage_get(slot, 0);
	dst = dst ? dst : "";
	if(src && ret) {
		size_t src_len = strlen(src);
		size_t dst_len = strlen(dst);
		while(ret) {
			char *src_pos = NULL;
			char *copy_pos = NULL;
			char *rest_pos = NULL;
			size_t ret_len = strlen(ret);
			// We do this first since the string address might change after
			// reallocation, thus invalidating the strstr() result
			ret = strings_storage_get(slot, ret_len + dst_len);
			if(!ret) {
				break;
			}
			src_pos = strstr(ret, src);
			if(!src_pos) {
				break;
			}
			copy_pos = src_pos + dst_len;
			rest_pos = src_pos + src_len;
			memmove(copy_pos, rest_pos, strlen(rest_pos) + 1);
			memcpy(src_pos, dst, dst_len);
		}
	}
	// Try to save the situation at least somewhat...
	return ret ? ret : dst;
}

/// String lookup hooks
/// -------------------
int WINAPI strings_MessageBoxA(
	HWND hWnd,
	LPCSTR lpText,
	LPCSTR lpCaption,
	UINT uType
)
{
	lpText = strings_lookup(lpText, NULL);
	lpCaption = strings_lookup(lpCaption, NULL);
	return chain_MessageBoxU(hWnd, lpText, lpCaption, uType);
}
/// -------------------

void strings_mod_init(void)
{
	jsondata_add("stringdefs.js");
	jsondata_game_add("stringlocs.js");
	strings_storage = json_object();
}

void strings_mod_detour(void)
{
	detour_chain("user32.dll", 1,
		"MessageBoxA", strings_MessageBoxA, &chain_MessageBoxU,
		NULL
	);
}

void strings_mod_exit(void)
{
	const char *key;
	json_t *val;

	json_object_foreach(strings_storage, key, val) {
		storage_string_t *p = (storage_string_t*)json_hex_value(val);
		SAFE_FREE(p);
	}
	strings_storage = json_decref_safe(strings_storage);
}
