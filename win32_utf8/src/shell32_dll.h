/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * shell32.dll functions.
  */

#pragma once

BOOL SHGetPathFromIDListU(
	__in PCIDLIST_ABSOLUTE pidl,
	__out_ecount(MAX_PATH) LPSTR pszPath
);
#undef SHGetPathFromIDList
#define SHGetPathFromIDList SHGetPathFromIDListU
