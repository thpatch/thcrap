/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Translation of hardcoded strings.
  */

#include <thcrap.h>

static json_t *stringdefs = NULL;
static json_t *stringlocs = NULL;
static json_t *sprintf_storage = NULL;

#define addr_key_len 2 + 8 + 1

const char* strings_get(const char *id)
{
	return json_object_get_string(stringdefs, id);
}

const char* strings_lookup(const char *in, size_t *out_len)
{
	char addr_key[addr_key_len];
	const char *id_key = NULL;
	const char *ret = in;

	if(!in) {
		return in;
	}

	snprintf(addr_key, addr_key_len, "0x%x", in);
	id_key = json_object_get_string(stringlocs, addr_key);
	if(id_key) {
		const char *new_str = strings_get(id_key);
		if(new_str && new_str[0]) {
			ret = new_str;
		}
	}
	if(out_len) {
		*out_len = strlen(ret) + 1;
	}
	return ret;
}

const char* strings_vsprintf(const size_t addr, const char *format, va_list va)
{
	char *ret = NULL;
	size_t str_len;
	char addr_key[addr_key_len];

	format = strings_lookup(format, NULL);

	if(!format) {
		return NULL;
	}
	str_len = _vscprintf(format, va) + 1;

	// We shouldn't use JSON strings here because Jansson forces them to be
	// in UTF-8, and we'd like to sprintf regardless of encoding.
	// Thus, we have to store char pointers as JSON integers and reallocate
	// memory if necessary.

	sprintf(addr_key, "0x%x", addr);
	ret = (char*)json_object_get_hex(sprintf_storage, addr_key);

	// MSVCRT's realloc implementation moves the buffer every time, even if the
	// new length is shorter...
	if(!ret || (strlen(ret) + 1 < str_len)) {
		ret = (char*)realloc(ret, str_len);
		json_object_set_new(sprintf_storage, addr_key, json_integer((size_t)ret));
	}
	if(ret) {
		vsprintf(ret, format, va);
		return ret;
	} else {
		// Try to save the situation at least somewhat...
		return format;
	}
}

const char* strings_sprintf(const size_t addr, const char *format, ...)
{
	va_list va;
	const char *ret;
	va_start(va, format);
	ret = strings_vsprintf(addr, format, va);
	return ret;
}

/// String lookup hooks
/// -------------------
int WINAPI strings_MessageBoxA(
	__in_opt HWND hWnd,
	__in_opt LPCSTR lpText,
	__in_opt LPCSTR lpCaption,
	__in UINT uType
)
{
	lpText = strings_lookup(lpText, NULL);
	lpCaption = strings_lookup(lpCaption, NULL);
	return MessageBoxU(hWnd, lpText, lpCaption, uType);
}
/// -------------------

void strings_init()
{
	stringdefs = stack_json_resolve("stringdefs.js", NULL);
	stringlocs = stack_game_json_resolve("stringlocs.js", NULL);
	sprintf_storage = json_object();
}

int strings_patch(HMODULE hMod)
{
	return iat_patch_funcs_var(hMod, "user32.dll", 1,
		"MessageBoxA", strings_MessageBoxA
	);
}

void strings_exit()
{
	const char *key;
	json_t *val;

	json_decref(stringdefs);
	json_decref(stringlocs);
	json_object_foreach(sprintf_storage, key, val) {
		free((void*)json_hex_value(val));
	}
	json_decref(sprintf_storage);
}
