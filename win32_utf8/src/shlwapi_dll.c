/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * shlwapi.dll functions.
  */

#include <Shlwapi.h>
#include "win32_utf8.h"

BOOL STDAPICALLTYPE PathMatchSpecU(
	__in LPCSTR pszFile, __in LPCSTR pszSpec
)
{
	BOOL ret;
	WCHAR_T_DEC(pszFile);
	WCHAR_T_DEC(pszSpec);
	StringToUTF16(pszFile_w, pszFile, pszFile_len);
	StringToUTF16(pszSpec_w, pszSpec, pszSpec_len);
	ret = PathMatchSpecW(pszFile_w, pszSpec_w);
	VLA_FREE(pszFile_w);
	VLA_FREE(pszSpec_w);
	return ret;
}

BOOL STDAPICALLTYPE PathRemoveFileSpecU(
	__inout LPSTR pszPath
)
{
	// Hey, let's re-write the function to also handle forward slashes
	// while we're at it!
	LPSTR newPath = PathFindFileNameA(pszPath);
	if((newPath) && (newPath != pszPath)) {
		newPath[0] = TEXT('\0');
		return 1;
	}
	return 0;
}

BOOL STDAPICALLTYPE PathFileExistsU(
	__in LPCSTR pszPath
)
{
	BOOL ret;
	WCHAR_T_DEC(pszPath);
	StringToUTF16(pszPath_w, pszPath, pszPath_len);
	ret = PathFileExistsW(pszPath_w);
	VLA_FREE(pszPath_w);
	return ret;
}
