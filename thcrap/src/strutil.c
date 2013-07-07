/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * String utility functions.
  */

#include "thcrap.h"

void str_slash_normalize(char *str)
{
	unsigned int i;
	for(i = 0; i < strlen(str); i++) {
		if(str[i] == '\\') {
			str[i] = '/';
		}
	}
}

void str_slash_normalize_win(char *str)
{
	unsigned int i;
	for(i = 0; i < strlen(str); i++) {
		if(str[i] == L'/') {
			str[i] = L'\\';
		}
	}
}

unsigned int str_num_digits(int number)
{
	unsigned int digits = 0;
	if (number < 0) {
		digits = 1; // remove this line if '-' counts as a digit
	}
	while (number) {
		number /= 10;
		digits++;
	}
	return digits;
}

int str_num_base(const char *str)
{
	return !strnicmp(str, "0x", 2) ? 16 : 10;
}

void str_hexdate_format(char format[11], DWORD date)
{
	sprintf(format, "%04x-%02x-%02x",
		(date & 0xffff0000) >> 16,
		(date & 0x0000ff00) >> 8,
		(date & 0x000000ff)
	);
}
