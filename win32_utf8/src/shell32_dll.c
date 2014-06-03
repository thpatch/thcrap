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
