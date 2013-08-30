/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * psapi.dll functions.
  */

#include "win32_utf8.h"

DWORD WINAPI GetModuleFileNameExU(
	__in HANDLE hProcess,
	__in_opt HMODULE hModule,
	__out_ecount(nSize) LPSTR lpFilename,
	__in DWORD nSize
)
{
	VLA(wchar_t, lpFilename_w, nSize);
	DWORD ret = GetModuleFileNameExW(hProcess, hModule, lpFilename_w, nSize);
	StringToUTF8(lpFilename, lpFilename_w, nSize);
	VLA_FREE(lpFilename_w);
	return ret;
}
