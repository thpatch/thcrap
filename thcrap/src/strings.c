/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Translation of hardcoded strings.
  */

#include "thcrap.h"

static json_t *stringdefs = NULL;
static json_t *stringlocs = NULL;
static json_t *strings_storage = NULL;

#define addr_key_len 2 + (sizeof(void*) * 2) + 1

const char* strings_get(const char *id)
{
	return json_object_get_string(stringdefs, id);
}

const char* strings_lookup(const char *in, size_t *out_len)
{
	const char *id_key = NULL;
	const char *ret = in;

	if(!in) {
		return in;
	}

	id_key = json_string_value(json_object_hexkey_get(stringlocs, (size_t)in));
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

va_list printf_parse_num(va_list va, const char **p)
{
	if(**p == '*') {
		// width specified in next variable argument
		(*p)++;
		va_arg(va, int);
	} else while(isdigit(**p)) {
		(*p)++;
	}
	return va;
}

// The printf format parsing below is based on Wine's implementation for
// msvcrt.dll (dlls/msvcrt/printf.h).
void strings_va_lookup(va_list va, const char *format)
{
	const char *p = format;
	while(*p) {
		char fmt;
		int flag_double = 0;

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

		// Skip flags. From left to right:
		// prefix sign, prefix space, left-align, zero padding, alternate
		while(strchr("+ -0#", *p) != NULL) {
			p++;
		}

		// Width
		va = printf_parse_num(va, &p);

		// Precision
		if(*p == '.') {
			p++;
			va = printf_parse_num(va, &p);
		}

		// Argument size modifier
		while(*p) {
			if(*p=='l' && *(p+1)=='l') {
				flag_double = 1;
				p += 2;
			} else if(*p=='h' || *p=='l' || *p=='L') {
				p++;
			} else if(*p == 'I') {
				if(*(p+1)=='6' && *(p+2)=='4') {
					flag_double = 1;
					p += 3;
				} else if(*(p+1)=='3' && *(p+2)=='2') {
					p += 3;
				} else if(isdigit(*(p+1)) || !*(p+1)) {
					break;
				} else {
					p++;
				}
			} else if(*p == 'w' || *p == 'F') {
				p++;
			} else {
				break;
			}
		}
		fmt = *p;
		if(fmt == 's' || fmt == 'S') {
			*(const char**)va = strings_lookup(*(const char**)va, NULL);
		}
		// Advance [va] if the format is among the valid ones
		if(strchr("aeEfgG", fmt)) {
			va_arg(va, double);
		}
		if(strchr("sScCpndiouxX", fmt)) {
			va_arg(va, int);
			if(flag_double) {
				va_arg(va, int);
			}
		}
		p++;
	}
}

char* strings_storage_get(const size_t slot, size_t min_len)
{
	char *ret = NULL;
	char addr_key[addr_key_len];

	itoa(slot, addr_key, 16);
	ret = (char*)json_object_get_hex(strings_storage, addr_key);

	// MSVCRT's realloc implementation moves the buffer every time, even if the
	// new length is shorter...
	if(!ret || (strlen(ret) + 1 < min_len)) {
		ret = (char*)realloc(ret, min_len);
		// Yes, this correctly handles a realloc failure.
		if(ret) {
			json_object_set_new(strings_storage, addr_key, json_integer((size_t)ret));
		}
	}
	return ret;
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

void strings_init(void)
{
	stringdefs = stack_json_resolve("stringdefs.js", NULL);
	stringlocs = stack_game_json_resolve("stringlocs.js", NULL);
	strings_storage = json_object();
}

int strings_detour(HMODULE hMod)
{
	return iat_detour_funcs_var(hMod, "user32.dll", 1,
		"MessageBoxA", strings_MessageBoxA
	);
}

void strings_exit(void)
{
	const char *key;
	json_t *val;

	json_decref(stringdefs);
	json_decref(stringlocs);
	json_object_foreach(strings_storage, key, val) {
		free((void*)json_hex_value(val));
	}
	json_decref(strings_storage);
}
