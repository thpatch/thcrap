/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Random utility functions.
  */

#include "thcrap.h"
#include <errno.h>

size_t dword_align(const size_t val)
{
	return (val + 3) & ~3;
}

BYTE* ptr_dword_align(const BYTE *in)
{
	return (BYTE*)dword_align((UINT_PTR)in);
}

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

void str_ascii_replace(char *str, const char from, const char to)
{
	if(str) {
		char *p = str;
		while(*p) {
			if(p[0] == from) {
				p[0] = to;
			}
			p++;
		}
	}
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
	return !strnicmp(str, "0x", 2) ? 16 : 10;
}

void str_hexdate_format(char format[11], uint32_t date)
{
	sprintf(format, "%04x-%02x-%02x",
		(date & 0xffff0000) >> 16,
		(date & 0x0000ff00) >> 8,
		(date & 0x000000ff)
	);
}

size_t str_address_value(const char *str, HMODULE hMod, uint8_t *error)
{
	int base = 10;
	size_t offset = 0;
	size_t ret = 0;
	char *endptr;

	if(str[0] != '\0' && str[1] != '\0' && str[2] != '\0') {
		// Module-relative hex values
		if(!strnicmp(str, "Rx", 2)) {
			ret += (size_t)(hMod ? hMod : GetModuleHandle(NULL));
			base = 16;
			offset = 2;
		} else if(!strnicmp(str, "0x", 2)) {
			base = 16;
			offset = 2;
		}
	}
	errno = 0;
	ret += strtol(str + offset, &endptr, base);

	if(error) {
		*error = STR_ADDRESS_ERROR_NONE;

		if(errno == ERANGE) {
			*error |= STR_ADDRESS_ERROR_OVERFLOW;
		}
		if(*endptr != '\0') {
			*error |= STR_ADDRESS_ERROR_GARBAGE;
		}
	}
	return ret;
}
