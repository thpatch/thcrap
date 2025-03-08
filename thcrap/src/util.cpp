/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Random utility functions.
  */

#include "thcrap.h"

void (str_slash_normalize)(char *str)
{
	str_slash_normalize_inline(str);
}

void (str_slash_normalize_win)(char *str)
{
	str_slash_normalize_win_inline(str);
}

unsigned int (str_num_digits)(ssize_t number)
{
	return str_num_digits_inline(number);
}

int (str_num_base)(const char *str)
{
	return str_num_base_inline(str);
}

void (str_hexdate_format)(char buffer[11], uint32_t date)
{
	str_hexdate_format_inline(buffer, date);
}

TH_NOINLINE int TH_VECTORCALL ascii_stricmp(const char* str1, const char* str2) {
	unsigned char c1, c2;
	for (size_t i = 0;; ++i) {
		c1 = ((unsigned char*)str1)[i];
		c1 |= (c1 - 'A' < 26) << 5;
		c2 = ((unsigned char*)str2)[i];
		c2 |= (c2 - 'A' < 26) << 5;
		if ((c1 -= c2) || !c2) {
			break;
		}
	}
	return (signed char)c1;
}

TH_NOINLINE int TH_VECTORCALL ascii_strnicmp(const char* str1, const char* str2, size_t count) {
	unsigned char c1, c2;
	TH_OPTIMIZING_ASSERT(count > 0);
	for (size_t i = 0; i < count; ++i) {
		c1 = ((unsigned char*)str1)[i];
		c1 |= (c1 - 'A' < 26) << 5;
		c2 = ((unsigned char*)str2)[i];
		c2 |= (c2 - 'A' < 26) << 5;
		if ((c1 -= c2) || !c2) {
			break;
		}
	}
	return (signed char)c1;
}

bool (is_valid_hex)(char c) {
	return is_valid_hex_inline(c);
}

int8_t (hex_value)(char c) {
	return hex_value_inline(c);
}

int _vasprintf(char** buffer_ret, const char* format, va_list va) {
	va_list va2;

	va_copy(va2, va);
	int length = vsnprintf(NULL, 0, format, va2);
	va_end(va2);

	char* buffer = (char*)malloc(length + 1);

	va_copy(va2, va);
	int ret = vsprintf(buffer, format, va2);
	va_end(va2);

	if (ret != -1) {
		*buffer_ret = buffer;
	} else {
		free(buffer);
		*buffer_ret = NULL;
	}

	return ret;
}

int _asprintf(char** buffer_ret, const char* format, ...) {
	va_list va;
	va_start(va, format);
	int ret = _vasprintf(buffer_ret, format, va);
	va_end(va);
	return ret;
}
