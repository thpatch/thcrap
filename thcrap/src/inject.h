/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * DLL injection.
  */

#pragma once

/// Entry point determination
/// -------------------------
// After creating a process in suspended state, EAX is guaranteed to contain
// the correct address of the entry point, even when the executable has the
// DYNAMICBASE flag activated in its header.
//
// (Works on Windows, but not on Wine)
void* entry_from_context(HANDLE hThread);
/// -------------------------

int ThreadWaitUntil(HANDLE hProcess, HANDLE hThread, void *addr);
int WaitUntilEntryPoint(HANDLE hProcess, HANDLE hThread, const char *module);

// CreateProcess with thcrap DLL injection.
// Careful! If you call this with CREATE_SUSPENDED set, you *must* resume the
// thread on your own! This is necessary for cooperation with other patches
// using DLL injection.
BOOL WINAPI inject_CreateProcessU(
	__in_opt LPCSTR lpAppName,
	__inout_opt LPSTR lpCmdLine,
	__in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
	__in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes,
	__in BOOL bInheritHandles,
	__in DWORD dwCreationFlags,
	__in_opt LPVOID lpEnvironment,
	__in_opt LPCSTR lpCurrentDirectory,
	__in LPSTARTUPINFOA lpSI,
	__out LPPROCESS_INFORMATION lpPI
);
BOOL WINAPI inject_CreateProcessW(
	__in_opt LPCWSTR lpAppName,
	__inout_opt LPWSTR lpCmdLine,
	__in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
	__in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes,
	__in BOOL bInheritHandles,
	__in DWORD dwCreationFlags,
	__in_opt LPVOID lpEnvironment,
	__in_opt LPCWSTR lpCurrentDirectory,
	__in LPSTARTUPINFOW lpSI,
	__out LPPROCESS_INFORMATION lpPI
);

// Catch DLL injection (lpStartAddress == LoadLibraryA() or LoadLibraryW())
// and redirect to our modified versions that detour all necessary functions.
__out_opt HANDLE WINAPI inject_CreateRemoteThread(
	__in HANDLE hProcess,
	__in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes,
	__in SIZE_T dwStackSize,
	__in LPTHREAD_START_ROUTINE lpStartAddress,
	__in_opt LPVOID lpParameter,
	__in DWORD dwCreationFlags,
	__out_opt LPDWORD lpThreadId
);

HMODULE WINAPI inject_LoadLibraryU(
	__in LPCSTR lpLibFileName
);
HMODULE WINAPI inject_LoadLibraryW(
	__in LPCWSTR lpLibFileName
);

void inject_mod_detour(void);

// Injects thcrap into the given [hProcess], and passes [run_cfg_fn].
int thcrap_inject(HANDLE hProcess, const char *run_cfg_fn);
