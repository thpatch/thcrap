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
