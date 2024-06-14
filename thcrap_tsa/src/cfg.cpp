/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * .cfg file related bugfixes
  */
#include <thcrap.h>
extern "C" {
#include "thcrap_tsa.h"
}

static auto chain_CreateFileA = CreateFileU;
static auto chain_ReadFile = ReadFile;
static auto chain_WriteFile = WriteFile;

static bool cfg_loaded = false;
static HANDLE cfg_handle = INVALID_HANDLE_VALUE;

HANDLE WINAPI cfg_CreateFileA(
	LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,	DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes, HANDLE hTemplateFile
)
{
	if (PathMatchSpecExU(lpFileName, "*.cfg", PMSF_NORMAL) == S_OK) {
		cfg_handle = chain_CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, OPEN_ALWAYS, dwFlagsAndAttributes, hTemplateFile);
		return cfg_handle;
	}
	else {
		return chain_CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	}
}

BOOL WINAPI cfg_ReadFile(
	HANDLE hFile, LPVOID lpBuffer,
	DWORD nNumberOfBytesToRead,
	LPDWORD lpNumberOfBytesRead,
	LPOVERLAPPED lpOverlapped
)
{
	BOOL ret = chain_ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
	if (!cfg_loaded && ret && hFile == cfg_handle) {
		cfg_loaded = true;
	}
	return ret;
}

BOOL WINAPI cfg_WriteFile(
	HANDLE hFile, LPCVOID lpBuffer,
	DWORD nNumberOfBytesToWrite,
	LPDWORD lpNumberOfBytesWritten,
	LPOVERLAPPED lpOverlapped
)
{
	if (!cfg_loaded && hFile == cfg_handle)
		return FALSE;
	return chain_WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
}

extern "C" TH_EXPORT void cfg_mod_detour(void) {
	if (!is_custom) {
		detour_chain("kernel32.dll", 1,
			"CreateFileA", cfg_CreateFileA, &chain_CreateFileA,
			"ReadFile", cfg_ReadFile, &chain_ReadFile,
			"WriteFile", cfg_WriteFile, &chain_WriteFile,
			nullptr
		);
	}
}
