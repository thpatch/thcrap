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

#define STRLEN_DEC(src_char) \
	size_t src_char##_len = strlen(src_char) + 1

/// Convenient wchar_t conversion macros
/// ------------------------------------
// "create-wchar_t-from-strlen"
#define WCHAR_T_DEC(src_char) \
	STRLEN_DEC(src_char); \
	VLA(wchar_t, src_char##_w, src_char##_len)

// StringToUTF16 using standard variable names
#define WCHAR_T_CONV(src_char) \
	StringToUTF16(src_char##_w, src_char, src_char##_len)
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

// Our strlen has error-checking!
#define strlen(s) (s ? strlen(s) : 0)

// Convenience macro to convert one fixed-length string to UTF-16.
// TODO: place this somewhere else?
#define FixedLengthStringConvert(str_in, str_len) \
	size_t str_in##_len = (str_len != -1 ? str_len : strlen(str_in)) + 1; \
	VLA(wchar_t, str_in##_w, str_in##_len); \
	ZeroMemory(str_in##_w, str_in##_len * sizeof(wchar_t)); \
	StringToUTF16(str_in##_w, str_in, str_len);
