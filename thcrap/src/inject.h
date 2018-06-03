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
	LPCSTR lpAppName,
	LPSTR lpCmdLine,
	LPSECURITY_ATTRIBUTES lpProcessAttributes,
	LPSECURITY_ATTRIBUTES lpThreadAttributes,
	BOOL bInheritHandles,
	DWORD dwCreationFlags,
	LPVOID lpEnvironment,
	LPCSTR lpCurrentDirectory,
	LPSTARTUPINFOA lpSI,
	LPPROCESS_INFORMATION lpPI
);
BOOL WINAPI inject_CreateProcessW(
	LPCWSTR lpAppName,
	LPWSTR lpCmdLine,
	LPSECURITY_ATTRIBUTES lpProcessAttributes,
	LPSECURITY_ATTRIBUTES lpThreadAttributes,
	BOOL bInheritHandles,
	DWORD dwCreationFlags,
	LPVOID lpEnvironment,
	LPCWSTR lpCurrentDirectory,
	LPSTARTUPINFOW lpSI,
	LPPROCESS_INFORMATION lpPI
);

// Catch DLL injection (lpStartAddress == LoadLibraryA() or LoadLibraryW())
// and redirect to our modified versions that detour all necessary functions.
HANDLE WINAPI inject_CreateRemoteThread(
	HANDLE hProcess,
	LPSECURITY_ATTRIBUTES lpThreadAttributes,
	SIZE_T dwStackSize,
	LPTHREAD_START_ROUTINE lpStartAddress,
	LPVOID lpParameter,
	DWORD dwCreationFlags,
	LPDWORD lpThreadId
);

HMODULE WINAPI inject_LoadLibraryU(
	LPCSTR lpLibFileName
);
HMODULE WINAPI inject_LoadLibraryW(
	LPCWSTR lpLibFileName
);

void inject_mod_detour(void);

// Injects thcrap into the given [hProcess], and passes [run_cfg_fn].
int thcrap_inject_into_running(HANDLE hProcess, const char *run_cfg_fn);

// Starts [exe_fn] as a new process with the given command-line arguments, and
// injects thcrap with the current run configuration into it.
BOOL thcrap_inject_into_new(const char *exe_fn, char *args, HANDLE *hProcess, HANDLE *hThread);
