#include "thcrap_wrapper.h"
#include <limits.h>

// A quick implementation of a few CRT functions because we don't link with the CRT.
size_t my_wcslen(const wchar_t *str)
{
	const wchar_t* str_read = str - 1;
	while (*++str_read);
	return str_read - str;
}

__declspec(noinline) int my_wcscmp(const wchar_t *s1, const wchar_t *s2)
{
	int d = *s1 - *s2;
	if (d) return d;
	while (*s1 && *s2 && *++s1 == *++s2);
	return *s1 - *s2;
}

// Returns the pointer to the end of dst, so that you can chain the call to append
// several strings.
LPWSTR my_strcpy(LPWSTR dst, LPCWSTR src)
{
	while (*dst++ = *src++);
	return dst - 1;
}

void *my_memcpy(void *dst, const void *src, size_t n)
{
	unsigned char* start = dst;
	unsigned char* const end = start + n;
	const unsigned char* source = src;

	while (start != end) {
		*start++ = *source++;
	}

	return start - n;
}

void *my_memset(void *dst, int ch, size_t n)
{
	unsigned char *start = dst;
	unsigned char *const end = start + n;
	
	// Structuring the loop like this with the assignment
	// between the if/do statements prevents the compiler
	// trying to substitute the loop with a call to memset
	// while also producing near-identical assembly to:
	// 
	// const unsigned char c = ch;
	// while (start != end) {
	//     _ReadWriteBarrier();
	//     *start++ = c;
	// }

	if (start != end) {
		const unsigned char c = ch;
		do {
			*start++ = c;
		} while (start != end);
	}

	return start - n;
}

void *my_alloc(size_t num, size_t size)
{
	if (num > SIZE_MAX / size)
		return NULL;

	return HeapAlloc(GetProcessHeap(), 0, num * size);
}

void my_free(void *ptr)
{
	if (ptr)
		HeapFree(GetProcessHeap(), 0, ptr);
}
