#include "thcrap_wrapper.h"

// A quick implementation of a few CRT functions because we don't link with the CRT.
size_t my_wcslen(const wchar_t *str)
{
	size_t n = 0;
	while (str[n]) {
		n++;
	}
	return n;
}

int my_wcscmp(const wchar_t *s1, const wchar_t *s2)
{
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}
	return *s2 - *s1;
}

// Returns the pointer to the end of dst, so that you can chain the call to append
// several strings.
LPWSTR my_strcpy(LPWSTR dst, LPCWSTR src)
{
	while (*src) {
		*dst = *src;
		src++;
		dst++;
	}
	*dst = '\0';
	return dst;
}

void *my_memcpy(void *dst, const void *src, size_t n)
{
	char *d = dst;
	const char *s = src;

	while (n-- > 0)
		*d++ = *s++;

	return dst;
}
