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

	_snprintf(addr_key, addr_key_len, "0x%x", in);
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
	const char *ret = NULL;
	size_t str_len;

	format = strings_lookup(format, NULL);

	if(!format) {
		return NULL;
	}
	str_len = _vscprintf(format, va) + 1;
	{
		VLA(char, str, str_len * UTF8_MUL);
		VLA(wchar_t, str_w, str_len);
		char addr_key[addr_key_len];

		sprintf(addr_key, "0x%x", addr);

		vsprintf(str, format, va);
		// Ensure UTF-8. *Very important*, since json_string verifies the string
		// and returns zero if it's invalid UTF-8!
		StringToUTF16(str_w, str, str_len);
		StringToUTF8(str, str_w, str_len);

		json_object_set_new(sprintf_storage, addr_key, json_string(str));

		ret = json_object_get_string(sprintf_storage, addr_key);
		if(!ret) {
			// Try to save the situation at least somewhat...
			ret = format;
		}
		VLA_FREE(str);
		VLA_FREE(str_w);
	}
	return ret;
}

const char* strings_sprintf(const size_t addr, const char *format, ...)
{
	va_list va;
	const char *ret;
	va_start(va, format);
	ret = strings_vsprintf(addr, format, va);
	return ret;
}

void strings_init()
{
	stringdefs = stack_json_resolve("stringdefs.js", NULL);
	stringlocs = stack_game_json_resolve("stringlocs.js", NULL);
	sprintf_storage = json_object();
}

void strings_exit()
{
	json_decref(stringdefs);
	json_decref(stringlocs);
	json_decref(sprintf_storage);
}
