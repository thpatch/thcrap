/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * kernel32.dll functions.
  */

#pragma once

BOOL WINAPI CreateDirectoryU(
	__in LPCSTR lpPathName,
	__in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes
);
#undef CreateDirectory
#define CreateDirectory CreateDirectoryU

HANDLE WINAPI CreateFileU(
	__in LPCSTR lpFileName,
	__in DWORD dwDesiredAccess,
	__in DWORD dwShareMode,
	__in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	__in DWORD dwCreationDisposition,
	__in DWORD dwFlagsAndAttributes,
	__in_opt HANDLE hTemplateFile
);
#undef CreateFile
#define CreateFile CreateFileU

BOOL WINAPI CreateProcessU(
	__in_opt LPCSTR lpApplicationName,
	__inout_opt LPSTR lpCommandLine,
	__in_opt PSECURITY_ATTRIBUTES lpProcessAttributes,
	__in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes,
	__in BOOL bInheritHandles,
	__in DWORD dwCreationFlags,
	__in_opt LPVOID lpEnvironment,
	__in_opt LPCSTR lpCurrentDirectory,
	__in LPSTARTUPINFOA lpStartupInfo,
	__out LPPROCESS_INFORMATION lpProcessInformation
);
#undef CreateProcess
#define CreateProcess CreateProcessU

HANDLE WINAPI FindFirstFileU(
	__in  LPCSTR lpFileName,
	__out LPWIN32_FIND_DATAA lpFindFileData
);
#undef FindFirstFile
#define FindFirstFile FindFirstFileU

BOOL WINAPI FindNextFileU(
	__in  HANDLE hFindFile,
	__out LPWIN32_FIND_DATAA lpFindFileData
);
#undef FindNextFile
#define FindNextFile FindNextFileU

DWORD WINAPI FormatMessageU(
	__in DWORD dwFlags,
	__in_opt LPCVOID lpSource,
	__in DWORD dwMessageId,
	__in DWORD dwLanguageId,
	__out LPSTR lpBuffer,
	__in DWORD nSize,
	__in_opt va_list *Arguments
);
#undef FormatMessage
#define FormatMessage FormatMessageU

DWORD WINAPI GetCurrentDirectoryU(
	__in DWORD nBufferLength,
	__out_ecount_part_opt(nBufferLength, return + 1) LPSTR lpBuffer
);
#undef GetCurrentDirectory
#define GetCurrentDirectory GetCurrentDirectoryU

DWORD WINAPI GetEnvironmentVariableU(
	__in_opt LPCSTR lpName,
	__out_ecount_part_opt(nSize, return + 1) LPSTR lpBuffer,
	__in DWORD nSize
);
#undef GetEnvironmentVariable
#define GetEnvironmentVariable GetEnvironmentVariableU

DWORD WINAPI GetFileAttributesU(
	__in LPCSTR lpFileName
);
#undef GetFileAttributes
#define GetFileAttributes GetFileAttributesU

BOOL WINAPI GetFileAttributesExU(
	__in LPCSTR lpFileName,
	__in GET_FILEEX_INFO_LEVELS fInfoLevelId,
	__out LPVOID lpFileInformation
);
#undef GetFileAttributesEx
#define GetFileAttributesEx GetFileAttributesExU

DWORD WINAPI GetModuleFileNameU(
	__in_opt HMODULE hModule,
	__out_ecount_part(nSize, return + 1) LPSTR lpFilename,
	__in DWORD nSize
);
#undef GetModuleFileName
#define GetModuleFileName GetModuleFileNameU

UINT WINAPI GetPrivateProfileIntU(
	__in LPCSTR lpAppName,
	__in LPCSTR lpKeyName,
	__in INT nDefault,
	__in_opt LPCSTR lpFileName
);
#undef GetPrivateProfileInt
#define GetPrivateProfileInt GetPrivateProfileIntU

VOID WINAPI GetStartupInfoU(
	__out LPSTARTUPINFOA lpStartupInfo
);
#undef GetStartupInfo
#define GetStartupInfo GetStartupInfoU

DWORD WINAPI GetTempPathU(
	__in DWORD nBufferLength,
	__out_ecount_part_opt(nBufferLength, return + 1) LPSTR lpBuffer
);
#undef GetTempPath
#define GetTempPath GetTempPathU

// Only implemented using the fallback codepage, since UTF-8 has
// no way to differentiate between continuation bytes and end bytes.
BOOL WINAPI IsDBCSLeadByteFB(
	__in BYTE TestChar
);
#undef IsDBCSLeadByte
#define IsDBCSLeadByte IsDBCSLeadByteFB

HMODULE WINAPI LoadLibraryU(
	__in LPCSTR lpLibFileName
);
#undef LoadLibrary
#define LoadLibrary LoadLibraryU

BOOL WINAPI MoveFileU(
	__in LPCSTR lpExistingFileName,
	__in LPCSTR lpNewFileName
);
#undef MoveFile
#define MoveFile MoveFileU

BOOL WINAPI MoveFileExU(
	__in LPCSTR lpExistingFileName,
	__in_opt LPCSTR lpNewFileName,
	__in DWORD dwFlags
);
#undef MoveFileEx
#define MoveFileEx MoveFileExU

BOOL WINAPI MoveFileWithProgressU(
	__in LPCSTR lpExistingFileName,
	__in_opt LPCSTR lpNewFileName,
	__in_opt LPPROGRESS_ROUTINE lpProgressRoutine,
	__in_opt LPVOID lpData,
	__in DWORD dwFlags
);
#undef MoveFileWithProgress
#define MoveFileWithProgress MoveFileWithProgressU

int WINAPI MultiByteToWideCharU(
	__in UINT CodePage,
	__in DWORD dwFlags,
	__in_bcount(cbMultiByte) LPCSTR lpMultiByteStr,
	__in int cbMultiByte,
	__out_ecount_opt(cchWideChar) __transfer(lpMultiByteStr) LPWSTR lpWideCharStr,
	__in int cchWideChar
);

BOOL WINAPI RemoveDirectoryU(
	__in LPCSTR lpPathName
);
#undef RemoveDirectory
#define RemoveDirectory RemoveDirectoryU

BOOL WINAPI SetCurrentDirectoryU(
	__in LPCSTR lpPathName
);
#undef SetCurrentDirectory
#define SetCurrentDirectory SetCurrentDirectoryU

int WINAPI WideCharToMultiByteU(
	__in UINT CodePage,
	__in DWORD dwFlags,
	__in_ecount(cchWideChar) LPCWSTR lpWideCharStr,
	__in int cchWideChar,
	__out_bcount_opt(cbMultiByte) __transfer(lpWideCharStr) LPSTR lpMultiByteStr,
	__in int cbMultiByte,
	__in_opt LPCSTR lpDefaultChar,
	__out_opt LPBOOL lpUsedDefaultChar
);

// Patchers
int kernel32_init(HMODULE hMod);
void kernel32_exit(void);
