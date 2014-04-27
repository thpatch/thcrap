/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * Macros.
  */

#pragma once

// C versions
#if defined(__STDC__)
# define C89
# if defined(__STDC_VERSION__)
#  define C90
#  if (__STDC_VERSION__ >= 199409L)
#   define C94
#  endif
#  if (__STDC_VERSION__ >= 199901L)
#   define C99
#  endif
# endif
#endif

#if defined(_WIN32) && defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#elif !defined(C99)
#include <malloc.h>
#endif

// Most Win32 API functions return TRUE on success and FALSE on failure,
// requiring a separate call to GetLastError() to get the actual error code.
// This macro wraps these function calls to use the opposite, more sensible
// scheme, returning FALSE on success and automatically calling GetLastError()
// on failure.
#define W32_ERR_WRAP(x) \
	x ? 0 : GetLastError()

#define SAFE_FREE(x) \
	if(x) { \
		free(x); \
		x = NULL; \
	}

// Variable length arrays
#if defined(C99)
# define VLA(type, name, size) \
	type name##_vla[size]; \
	type *name = name##_vla /* to ensure that [name] is a modifiable lvalue */
# define VLA_FREE(name) 
#else
# define VLA(type, name, size) \
	type *name = (type*)_malloca((size) * sizeof(type))
# define VLA_FREE(name) \
	if(name) { \
		_freea(name); \
		name = NULL; \
	}
#endif

// Our strlen has error-checking!
#define strlen(s) (s ? strlen(s) : 0)
#define wcslen(s) (s ? wcslen(s) : 0)

/// Convenient wchar_t conversion macros
/// ------------------------------------
#define STRLEN_DEC(src_char) \
	size_t src_char##_len = strlen(src_char) + 1

#define WCSLEN_DEC(src_wchar) \
	size_t src_wchar##_len = (wcslen(src_wchar) * UTF8_MUL) + 1

// "create-wchar_t-from-strlen"
#define WCHAR_T_DEC(src_char) \
	STRLEN_DEC(src_char); \
	VLA(wchar_t, src_char##_w, src_char##_len)

#define WCHAR_T_CONV(src_char) \
	StringToUTF16(src_char##_w, src_char, src_char##_len)

#define WCHAR_T_CONV_VLA(src_char) \
	src_char##_w = StringToUTF16_VLA(src_char##_w, src_char, src_char##_len)

#define WCHAR_T_FREE(src_char) \
	VLA_FREE(src_char##_w)

// "create-UTF-8-from-wchar_t"
#define UTF8_DEC(src_wchar) \
	WCSLEN_DEC(src_wchar); \
	VLA(char, src_wchar##_utf8, src_wchar##_len)

#define UTF8_CONV(src_wchar) \
	StringToUTF8(src_wchar##_utf8, src_wchar, src_wchar##_len)

#define UTF8_FREE(src_wchar) \
	VLA_FREE(src_wchar##_utf8)
/// ------------------------------------

// Define Visual C++ warnings away
#if (_MSC_VER >= 1600)
# define itoa _itoa
#ifndef strdup
# define strdup _strdup
#endif
# define snprintf _snprintf
# define strnicmp _strnicmp
# define stricmp _stricmp
# define strlwr _strlwr
# define vsnwprintf _vsnwprintf
# define wcsicmp _wcsicmp
#endif

// Convenience macro to convert one fixed-length string to UTF-16.
// TODO: place this somewhere else?
#define FixedLengthStringConvert(str_in, str_len) \
	size_t str_in##_len = (str_len != -1 ? str_len : strlen(str_in)) + 1; \
	VLA(wchar_t, str_in##_w, str_in##_len); \
	ZeroMemory(str_in##_w, str_in##_len * sizeof(wchar_t)); \
	StringToUTF16(str_in##_w, str_in, str_len);
