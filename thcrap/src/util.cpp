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

size_t str_address_value(const char *str, HMODULE CustomModuleBase, str_address_ret_t *ret)
{
	errno = 0;

	int base = 0;
	size_t val = 0;
	char **endptr = ret ? (char **)&ret->endptr : NULL;

	switch (str[0] | 0x20) {
		case 'r':
			val = (CustomModuleBase != NULL ? (uintptr_t)CustomModuleBase : (uintptr_t)CurrentImageBase);
			[[fallthrough]];
		case '0':
		{
			const bool is_hex = (*++str | 0x20) == 'x';
			str += is_hex;
			base = is_hex ? 16 : 10;
		}
	}

	val += strtouz(str, endptr, base);
	
	if(ret) {
		ret->error = STR_ADDRESS_ERROR_NONE;

		if(errno == ERANGE) {
			ret->error |= STR_ADDRESS_ERROR_OVERFLOW;
		}
		if(endptr && *endptr[0] != '\0') {
			ret->error |= STR_ADDRESS_ERROR_GARBAGE;
		}
	}
	return val;
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
