/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * shlwapi.dll functions.
  */

#pragma once

BOOL STDAPICALLTYPE PathMatchSpecU(
	__in LPCSTR pszFile, __in LPCSTR pszSpec
);
#undef PathMatchSpec
#define PathMatchSpec PathMatchSpecU

BOOL STDAPICALLTYPE PathFileExistsU(
	__in LPCSTR pszPath
);
#undef PathFileExists
#define PathFileExists PathFileExistsU

BOOL STDAPICALLTYPE PathRemoveFileSpecU(
	__inout LPSTR pszPath
);
#undef PathRemoveFileSpec
#define PathRemoveFileSpec PathRemoveFileSpecU
