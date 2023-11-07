/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Random utility functions.
  */

#include "thcrap.h"

size_t ptr_advance(const unsigned char **src, size_t num)
{
	*src += num;
	return num;
}

size_t memcpy_advance_src(unsigned char *dst, const unsigned char **src, size_t num)
{
	memcpy(dst, *src, num);
	return ptr_advance(src, num);
}

void str_slash_normalize(char *str)
{
	str_ascii_replace(str, '\\', '/');
}

void str_slash_normalize_win(char *str)
{
	str_ascii_replace(str, '/', '\\');
}

unsigned int str_num_digits(int number)
{
	unsigned int digits = 0;
	if(number < 0) {
		digits = 1; // remove this line if '-' counts as a digit
	}
	while(number) {
		number /= 10;
		digits++;
	}
	return digits;
}

int str_num_base(const char *str)
{
	return (str[0] == '0' && (str[1] | 0x20) == 'x') ? 16 : 10;
}

void str_hexdate_format(char format[11], uint32_t date)
{
	sprintf(format, "%04x-%02x-%02x",
		(date & 0xffff0000) >> 16,
		(date & 0x0000ff00) >> 8,
		(date & 0x000000ff)
	);
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

bool is_valid_hex(char c) {
	c |= 0x20;
	return is_valid_decimal(c) | ((uint8_t)(c - 'a') < 6);
}

int8_t hex_value(char c) {
	c |= 0x20;
	c -= '0';
	if ((uint8_t)c < 10) return c;
	c -= 49;
	if ((uint8_t)c < 6) return c + 10;
	return -1;
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
