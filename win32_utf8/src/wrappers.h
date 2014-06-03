/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * Generic call wrappers to cut down redundancy.
  */

#pragma once

// Wrapper for functions that take only a single string parameter.
typedef DWORD (WINAPI *Wrap1PFunc_t)(
	__in LPCWSTR lpsz
);
DWORD WINAPI Wrap1P(
	__in Wrap1PFunc_t func,
	__in LPCSTR lpsz
);

// Wrapper for functions that write a string into a buffer, and return its
// necessary size when passing 0 for [nBufferLength].
typedef DWORD (WINAPI *WrapGetStringFunc_t)(
	__in DWORD nBufferLength,
	__out_ecount_part_opt(nBufferLength, return + 1) LPWSTR lpBuffer
);
DWORD WINAPI WrapGetString(
	__in WrapGetStringFunc_t func,
	__in DWORD nBufferLength,
	__out_ecount_part_opt(nBufferLength, return + 1) LPSTR lpBuffer
);
