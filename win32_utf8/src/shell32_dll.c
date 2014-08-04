/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * shell32.dll functions.
  */

#pragma once

#include "win32_utf8.h"
#include "wrappers.h"

// The HDROP type would be he only reason for #include <shellapi.h> here,
// so let's declare the function ourselves to reduce header bloat.
SHSTDAPI_(UINT) DragQueryFileW(
	__in HANDLE hDrop,
	__in UINT iFile,
	__out_ecount_opt(cch) LPWSTR lpszFile,
	__in UINT cch
);

UINT DragQueryFileU(
	__in HANDLE hDrop,
	__in UINT iFile,
	__out_ecount_opt(cch) LPSTR lpszFile,
	__in UINT cch
)
{
	DWORD ret;
	VLA(wchar_t, lpszFile_w, cch);

	if(!lpszFile) {
		VLA_FREE(lpszFile_w);
	}
	ret = DragQueryFileW(hDrop, iFile, lpszFile_w, cch);
	if(ret) {
		if(lpszFile) {
			StringToUTF8(lpszFile, lpszFile_w, cch);
		} else if(iFile != 0xFFFFFFFF) {
			VLA(wchar_t, lpBufferReal_w, ret);
			ret = DragQueryFileW(hDrop, iFile, lpBufferReal_w, cch);
			ret = StringToUTF8(NULL, lpBufferReal_w, 0);
			VLA_FREE(lpBufferReal_w);
		}
	}
	VLA_FREE(lpszFile_w);
	return ret;
}

BOOL SHGetPathFromIDListU(
	__in PCIDLIST_ABSOLUTE pidl,
	__out_ecount(MAX_PATH) LPSTR pszPath
)
{
	wchar_t pszPath_w[MAX_PATH];
	BOOL ret = SHGetPathFromIDListW(pidl, pszPath_w);
	if(pszPath) {
		StringToUTF8(pszPath, pszPath_w, MAX_PATH);
		return ret;
	}
	return 0;
}
