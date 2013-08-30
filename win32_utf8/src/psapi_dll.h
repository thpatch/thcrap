/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * psapi.dll functions.
  */

#pragma once

DWORD WINAPI GetModuleFileNameExU(
	__in HANDLE hProcess,
	__in_opt HMODULE hModule,
	__out_ecount(nSize) LPSTR lpFilename,
	__in DWORD nSize
);
#undef GetModuleFileNameEx
#define GetModuleFileNameEx GetModuleFileNameExU
