/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * kernel32.dll functions.
  */

#include "win32_utf8.h"
#include "wrappers.h"

// GetStartupInfo
// --------------
static char *startupinfo_desktop = NULL;
static char *startupinfo_title = NULL;
// --------------

BOOL WINAPI CreateDirectoryU(
	__in LPCSTR lpPathName,
	__in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes
)
{
	// Hey, let's make this recursive while we're at it.
	BOOL ret;
	size_t i;
	size_t lpPathName_w_len;
	WCHAR_T_DEC(lpPathName);
	WCHAR_T_CONV_VLA(lpPathName);

	// no, this isn't optimized away
	lpPathName_w_len = wcslen(lpPathName_w);
	for(i = 0; i < lpPathName_w_len; i++) {
		if(lpPathName_w[i] == L'\\' || lpPathName_w[i] == L'/') {
			wchar_t old_c = lpPathName_w[i + 1];
			lpPathName_w[i + 1] = L'\0';
			lpPathName_w[i] = L'/';
			ret = CreateDirectoryW(lpPathName_w, NULL);
			lpPathName_w[i + 1] = old_c;
		}
	}
	// Final directory
	ret = CreateDirectoryW(lpPathName_w, NULL);
	WCHAR_T_FREE(lpPathName);
	return ret;
}

HANDLE WINAPI CreateFileU(
	__in LPCSTR lpFileName,
	__in DWORD dwDesiredAccess,
	__in DWORD dwShareMode,
	__in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	__in DWORD dwCreationDisposition,
	__in DWORD dwFlagsAndAttributes,
	__in_opt HANDLE hTemplateFile
)
{
	HANDLE ret;
	WCHAR_T_DEC(lpFileName);
	WCHAR_T_CONV_VLA(lpFileName);
	ret = CreateFileW(
		lpFileName_w, dwDesiredAccess, dwShareMode | FILE_SHARE_READ, lpSecurityAttributes,
		dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile
	);
	WCHAR_T_FREE(lpFileName);
	return ret;
}

BOOL WINAPI CreateProcessU(
	__in_opt LPCSTR lpAppName,
	__inout_opt LPSTR lpCmdLine,
	__in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
	__in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes,
	__in BOOL bInheritHandles,
	__in DWORD dwCreationFlags,
	__in_opt LPVOID lpEnvironment,
	__in_opt LPCSTR lpCurrentDirectory,
	__in LPSTARTUPINFOA lpSI,
	__out LPPROCESS_INFORMATION lpProcessInformation
)
{
	BOOL ret;
	STARTUPINFOW lpSI_w;
	WCHAR_T_DEC(lpAppName);
	WCHAR_T_DEC(lpCmdLine);
	WCHAR_T_DEC(lpCurrentDirectory);

	WCHAR_T_CONV_VLA(lpAppName);
	WCHAR_T_CONV_VLA(lpCmdLine);
	WCHAR_T_CONV_VLA(lpCurrentDirectory);

	if(lpSI) {
		size_t si_lpDesktop_len = strlen(lpSI->lpDesktop) + 1;
		VLA(wchar_t, si_lpDesktopW, si_lpDesktop_len);
		size_t si_lpTitle_len = strlen(lpSI->lpTitle) + 1;
		VLA(wchar_t, si_lpTitleW, si_lpTitle_len);

		// At least the structure sizes are identical here
		memcpy(&lpSI_w, lpSI, sizeof(STARTUPINFOW));
		si_lpDesktopW = StringToUTF16_VLA(si_lpDesktopW, lpSI->lpDesktop, si_lpDesktop_len);
		si_lpTitleW = StringToUTF16_VLA(si_lpTitleW, lpSI->lpTitle, si_lpTitle_len);

		lpSI_w.lpDesktop = si_lpDesktopW;
		lpSI_w.lpTitle = si_lpTitleW;
	} else {
		ZeroMemory(&lpSI_w, sizeof(STARTUPINFOW));
	}
	// "Set this member to NULL before passing the structure to CreateProcess,"
	// MSDN says.
	lpSI_w.lpReserved = NULL;
	ret = CreateProcessW(
		lpAppName_w,
		lpCmdLine_w,
		lpProcessAttributes,
		lpThreadAttributes,
		bInheritHandles,
		dwCreationFlags,
		lpEnvironment,
		lpCurrentDirectory_w,
		&lpSI_w,
		lpProcessInformation
	);
	VLA_FREE(lpSI_w.lpDesktop);
	VLA_FREE(lpSI_w.lpTitle);
	WCHAR_T_FREE(lpAppName);
	WCHAR_T_FREE(lpCmdLine);
	WCHAR_T_FREE(lpCurrentDirectory);
	return ret;
}

static void CopyFindDataWToA(
	__out LPWIN32_FIND_DATAA w32fd_a,
	__in LPWIN32_FIND_DATAW w32fd_w
	)
{
	w32fd_a->dwFileAttributes = w32fd_w->dwFileAttributes;
	w32fd_a->ftCreationTime = w32fd_w->ftCreationTime;
	w32fd_a->ftLastAccessTime = w32fd_w->ftLastAccessTime;
	w32fd_a->ftLastWriteTime = w32fd_w->ftLastWriteTime;
	w32fd_a->nFileSizeHigh = w32fd_w->nFileSizeHigh;
	w32fd_a->nFileSizeLow = w32fd_w->nFileSizeLow;
	w32fd_a->dwReserved0 = w32fd_w->dwReserved0;
	w32fd_a->dwReserved1 = w32fd_w->dwReserved1;
	StringToUTF8(w32fd_a->cFileName, w32fd_w->cFileName, sizeof(w32fd_a->cFileName));
	StringToUTF8(w32fd_a->cAlternateFileName, w32fd_w->cAlternateFileName, sizeof(w32fd_a->cAlternateFileName));
#ifdef _MAC
	w32fd_a->dwFileType = w32fd_w->dwReserved1;
	w32fd_a->dwCreatorType = w32fd_w->dwCreatorType;
	w32fd_a->wFinderFlags = w32fd_w->wFinderFlags;
#endif
}

HANDLE WINAPI FindFirstFileU(
	__in  LPCSTR lpFileName,
	__out LPWIN32_FIND_DATAA lpFindFileData
)
{
	HANDLE ret;
	DWORD last_error;
	WIN32_FIND_DATAW lpFindFileDataW;

	WCHAR_T_DEC(lpFileName);
	WCHAR_T_CONV_VLA(lpFileName);
	ret = FindFirstFileW(lpFileName_w, &lpFindFileDataW);
	last_error = GetLastError();
	CopyFindDataWToA(lpFindFileData, &lpFindFileDataW);
	SetLastError(last_error);
	WCHAR_T_FREE(lpFileName);
	return ret;
}

BOOL WINAPI FindNextFileU(
	__in  HANDLE hFindFile,
	__out LPWIN32_FIND_DATAA lpFindFileData
)
{
	BOOL ret;
	DWORD last_error;
	WIN32_FIND_DATAW lpFindFileDataW;

	ret = FindNextFileW(hFindFile, &lpFindFileDataW);
	last_error = GetLastError();
	CopyFindDataWToA(lpFindFileData, &lpFindFileDataW);
	SetLastError(last_error);
	return ret;
}

DWORD WINAPI FormatMessageU(
	__in DWORD dwFlags,
	__in_opt LPCVOID lpSource,
	__in DWORD dwMessageId,
	__in DWORD dwLanguageId,
	__out LPSTR lpBuffer,
	__in DWORD nSize,
	__in_opt va_list *Arguments
)
{
	wchar_t *lpBufferW = NULL;

	DWORD ret = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | dwFlags, lpSource,
		dwMessageId, dwLanguageId, (LPWSTR)&lpBufferW, nSize, Arguments
	);
	if(!ret) {
		return ret;
	}
	if(dwFlags & FORMAT_MESSAGE_ALLOCATE_BUFFER) {
		LPSTR* lppBuffer = (LPSTR*)lpBuffer;

		ret = max(ret * sizeof(char) * UTF8_MUL, nSize);

		*lppBuffer = LocalAlloc(0, ret);
		lpBuffer = *lppBuffer;
	} else {
		ret = min(ret, nSize);
	}
	ret = StringToUTF8(lpBuffer, lpBufferW, ret);
	LocalFree(lpBufferW);
	return ret;
}

DWORD WINAPI GetCurrentDirectoryU(
	__in DWORD nBufferLength,
	__out_ecount_part_opt(nBufferLength, return + 1) LPSTR lpBuffer
)
{
	DWORD ret;
	VLA(wchar_t, lpBuffer_w, nBufferLength);

	if(!lpBuffer) {
		VLA_FREE(lpBuffer_w);
	}
	ret = GetCurrentDirectoryW(nBufferLength, lpBuffer_w);
	if(lpBuffer) {
		StringToUTF8(lpBuffer, lpBuffer_w, nBufferLength);
	} else {
		// Hey, let's be nice and return the _actual_ length.
		VLA(wchar_t, lpBufferReal_w, ret);
		GetCurrentDirectoryW(ret, lpBufferReal_w);
		ret = StringToUTF8(NULL, lpBufferReal_w, 0);
		VLA_FREE(lpBufferReal_w);
	}
	VLA_FREE(lpBuffer_w);
	return ret;
}

DWORD WINAPI GetEnvironmentVariableU(
	__in_opt LPCSTR lpName,
	__out_ecount_part_opt(nSize, return + 1) LPSTR lpBuffer,
	__in DWORD nSize
)
{
	DWORD ret;
	WCHAR_T_DEC(lpName);
	VLA(wchar_t, lpBuffer_w, nSize);
	WCHAR_T_CONV_VLA(lpName);

	GetEnvironmentVariableW(lpName_w, lpBuffer_w, nSize);
	// Return the converted size (!)
	ret = StringToUTF8(lpBuffer, lpBuffer_w, nSize);
	VLA_FREE(lpBuffer_w);
	WCHAR_T_FREE(lpName);
	return ret;
}

#define INI_MACRO_EXPAND(macro) \
	macro(lpAppName); \
	macro(lpKeyName); \
	macro(lpFileName)

UINT WINAPI GetPrivateProfileIntU(
	__in LPCSTR lpAppName,
	__in LPCSTR lpKeyName,
	__in INT nDefault,
	__in_opt LPCSTR lpFileName
)
{
	UINT ret;
	INI_MACRO_EXPAND(WCHAR_T_DEC);
	INI_MACRO_EXPAND(WCHAR_T_CONV);
	ret = GetPrivateProfileIntW(lpAppName_w, lpKeyName_w, nDefault, lpFileName_w);
	INI_MACRO_EXPAND(WCHAR_T_FREE);
	return ret;
}

DWORD WINAPI GetModuleFileNameU(
	__in_opt HMODULE hModule,
	__out_ecount_part(nSize, return + 1) LPSTR lpFilename,
	__in DWORD nSize
)
{
	/**
	  * And here we are, the most stupid Win32 API function I've seen so far.
	  *
	  * This wrapper adds the "GetCurrentDirectory functionality" the original
	  * function unfortunately lacks. Pass NULL for [lpFilename] or [nSize] to
	  * get the size required for a buffer to hold the module name in UTF-8.
	  *
	  * ... and unless there is any alternative function I don't know of, the
	  * only way to actually calculate this size is to repeatedly increase a
	  * buffer and to check whether that has been enough.
	  *
	  * In practice though, this length should never exceed MAX_PATH. I failed to
	  * create any test case where the path would be larger. But just in case it
	  * is or this becomes more frequent some day, the code is here.
	  */

	DWORD ret = nSize ? nSize : MAX_PATH;
	VLA(wchar_t, lpFilename_w, ret);

	if(lpFilename && nSize) {
		GetModuleFileNameW(hModule, lpFilename_w, nSize);
	} else {
		BOOL error = 1;
		while(error) {
			GetModuleFileNameW(hModule, lpFilename_w, ret);
			error = GetLastError() == ERROR_INSUFFICIENT_BUFFER;
			if(error) {
				VLA(wchar_t, lpFilename_VLA, ret += MAX_PATH);
				VLA_FREE(lpFilename_w);
				lpFilename_w = lpFilename_VLA;
			}
		}
		nSize = 0;
	}
	ret = StringToUTF8(lpFilename, lpFilename_w, nSize);
	VLA_FREE(lpFilename_w);
	return ret;
}

VOID WINAPI GetStartupInfoU(
	__out LPSTARTUPINFOA lpSI
)
{
	STARTUPINFOW si_w;
	GetStartupInfoW(&si_w);

	// I would have put this code into kernel32_init, but apparently
	// GetStartupInfoW is "not safe to be called inside DllMain".
	// So unsafe in fact that Wine segfaults when I tried it
	if(!startupinfo_desktop) {
		size_t lpDesktop_len = wcslen(si_w.lpDesktop) + 1;
		startupinfo_desktop = (char*)malloc(lpDesktop_len * UTF8_MUL * sizeof(char));
		StringToUTF8(startupinfo_desktop, si_w.lpDesktop, lpDesktop_len);
	}
	if(!startupinfo_title) {
		size_t lpTitle_len = wcslen(si_w.lpTitle) + 1;
		startupinfo_title = (char*)malloc(lpTitle_len * UTF8_MUL * sizeof(char));
		StringToUTF8(startupinfo_title, si_w.lpTitle, lpTitle_len);
	}
	memcpy(lpSI, &si_w, sizeof(STARTUPINFOA));
	lpSI->lpDesktop = startupinfo_desktop;
	lpSI->lpTitle = startupinfo_title;
}

BOOL WINAPI IsDBCSLeadByteFB(
	__in BYTE TestChar
)
{
	extern UINT fallback_codepage;
	return IsDBCSLeadByteEx(fallback_codepage, TestChar);
}

HMODULE WINAPI LoadLibraryU(
	__in LPCSTR lpLibFileName
)
{
	return (HMODULE)Wrap1P((Wrap1PFunc_t)LoadLibraryW, lpLibFileName);
}

BOOL WINAPI MoveFileU(
	__in LPCSTR lpExistingFileName,
	__in LPCSTR lpNewFileName
)
{
	return MoveFileEx(lpExistingFileName, lpNewFileName, MOVEFILE_COPY_ALLOWED);
}

BOOL WINAPI MoveFileExU(
	__in LPCSTR lpExistingFileName,
	__in_opt LPCSTR lpNewFileName,
	__in DWORD dwFlags
)
{
	return MoveFileWithProgress(
		lpExistingFileName, lpNewFileName, NULL, NULL, dwFlags
	);
}

BOOL WINAPI MoveFileWithProgressU(
	__in LPCSTR lpExistingFileName,
	__in_opt LPCSTR lpNewFileName,
	__in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
	__in_opt LPVOID lpData,
	__in DWORD dwFlags
)
{
	BOOL ret;
	WCHAR_T_DEC(lpExistingFileName);
	WCHAR_T_DEC(lpNewFileName);
	WCHAR_T_CONV_VLA(lpExistingFileName);
	WCHAR_T_CONV_VLA(lpNewFileName);
	ret = MoveFileWithProgressW(
		lpExistingFileName_w, lpNewFileName_w, lpProgressRoutine, lpData, dwFlags
	);
	WCHAR_T_FREE(lpExistingFileName);
	WCHAR_T_FREE(lpNewFileName);
	return ret;
}

BOOL WINAPI SetCurrentDirectoryU(
	__in LPCSTR lpPathName
)
{
	return Wrap1P((Wrap1PFunc_t)SetCurrentDirectoryW, lpPathName);
}

// Patcher functions
// -----------------
int kernel32_init(HMODULE hMod)
{
	return 0;
}

void kernel32_exit(void)
{
	SAFE_FREE(startupinfo_desktop);
	SAFE_FREE(startupinfo_title);
}
