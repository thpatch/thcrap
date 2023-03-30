/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * DLL injection.
  */

#include "thcrap.h"

/// Detour chains
/// -------------
typedef BOOL WINAPI CreateProcessW_type(
	LPCWSTR lpApplicationName,
	LPWSTR lpCommandLine,
	LPSECURITY_ATTRIBUTES lpProcessAttributes,
	LPSECURITY_ATTRIBUTES lpThreadAttributes,
	BOOL bInheritHandles,
	DWORD dwCreationFlags,
	LPVOID lpEnvironment,
	LPCWSTR lpCurrentDirectory,
	LPSTARTUPINFOW lpStartupInfo,
	LPPROCESS_INFORMATION lpProcessInformation
);

typedef HANDLE WINAPI CreateRemoteThread_type(
	HANDLE hProcess,
	LPSECURITY_ATTRIBUTES lpThreadAttributes,
	SIZE_T dwStackSize,
	LPTHREAD_START_ROUTINE lpStartAddress,
	LPVOID lpParameter,
	DWORD dwCreationFlags,
	LPDWORD lpThreadId
);

typedef HMODULE WINAPI LoadLibraryW_type(
	LPCWSTR lpLibFileName
);

W32U8_DETOUR_CHAIN_DEF(CreateProcess);
DETOUR_CHAIN_DEF(CreateProcessW);
DETOUR_CHAIN_DEF(CreateRemoteThread);
W32U8_DETOUR_CHAIN_DEF(LoadLibrary);
DETOUR_CHAIN_DEF(LoadLibraryW);
/// -------------

/**
  * A more complete DLL injection solution.
  * Adapted from http://www.codeproject.com/Articles/20084/completeinject.
  *
  * Parameters
  * ----------
  *	HANDLE hProcess
  *		The handle to the process to inject the DLL into.
  *
  *	const char *dll_dir
  *		Directory of the DLL to inject. (optional)
  *		During injection, the target application's directory can be temporarily
  *		switched to this directory. This may be necessary if the injected DLL
  *		depends on other DLLs stored in the same directory.
  *
  *	const char *dll_fn
  *		File name of the DLL to inject into the process.
  *
  *	const char *func_name
  *		The name of the function to call once the DLL has been injected.
  *
  *	const void *param
  *		Optional parameter to pass to [func_name].
  *		Note that the pointer received in [func_name] points to a copy in the
  *		injection workspace and, thus, is only valid until the function returns.
  *		Don't store it globally in the DLL!
  *
  *	const size_t param_size
  *		Size of [param].
  *
  * Return value
  * ------------
  * Exit code of the injected function:
  *	0 for success
  *	1 if first error was thrown
  *	2 if second error was thrown
  *
  * Description
  * -----------
  * This function will inject a DLL into a process and execute an exported function
  * from the DLL to "initialize" it. The function should be in the format shown below,
  * one parameter and no return type. Do not forget to prefix extern "C" if you are in C++.
  *
  *		TH_EXPORT void FunctionName(void*)
  *
  * The function that is called in the injected DLL -MUST- return, the loader
  * waits for the thread to terminate before removing the allocated space and
  * returning control to the loader. This method of DLL injection also adds error
  * handling, so the end user knows if something went wrong.
  */

extern "C" {
	extern const uint8_t inject;
	extern const uint8_t inject_dlldirptr;
	extern const uint8_t inject_loadlibraryflagsptr;
	extern const uint8_t inject_dllnameptr;
	extern const uint8_t inject_funcnameptr;
	extern const uint8_t inject_funcparamptr;
	extern const uint8_t inject_end;

	extern const uint8_t inject_GetCurrentDirectoryWptr;
	extern const uint8_t inject_SetCurrentDirectoryWptr;
	extern const uint8_t inject_LoadLibraryExWptr;
	extern const uint8_t inject_ExitThreadptr;
	extern const uint8_t inject_GetProcAddressptr;
	extern const uint8_t inject_FreeLibraryAndExitThreadptr;
}

// Calculate all the offsets once and store them for later
static const size_t inject_size = &inject_end - &inject;
static const size_t inject_dlldir = &inject_dlldirptr + 1 - &inject;
static const size_t inject_loadlibraryflags = &inject_loadlibraryflagsptr + 1 - &inject;
static const size_t inject_dllname = &inject_dllnameptr + 1 - &inject;
static const size_t inject_funcname = &inject_funcnameptr + 1 - &inject;
static const size_t inject_funcparam = &inject_funcparamptr + 1 - &inject;

static const size_t inject_GetCurrentDirectoryW = &inject_GetCurrentDirectoryWptr + 1 - &inject;
static const size_t inject_SetCurrentDirectoryW = &inject_SetCurrentDirectoryWptr + 1 - &inject;
static const size_t inject_LoadLibraryExW = &inject_LoadLibraryExWptr + 1 - &inject;
static const size_t inject_ExitThread = &inject_ExitThreadptr + 1 - &inject;
static const size_t inject_GetProcAddress = &inject_GetProcAddressptr + 1 - &inject;
static const size_t inject_FreeLibraryAndExitThread = &inject_FreeLibraryAndExitThreadptr + 1 - &inject;

int Inject(const HANDLE hProcess, const wchar_t *const dll_dir, const wchar_t *const dll_fn, const char *const func_name, const void *const param, const size_t param_size)
{
	#define have_kb2533623 ((bool)GetProcAddress(GetModuleHandleA("kernel32.dll"), "SetDefaultDllDirectories"))

	size_t dll_dir_size = 0;
	size_t dll_dir_total_size = 0;
	if (dll_dir) {
		dll_dir_size = (wcslen(dll_dir) + 1) * 2;
		dll_dir_total_size = AlignUpToMultipleOf2(dll_dir_size, 4);
	}

	const size_t dll_fn_size = (wcslen(dll_fn) + 1) * 2;
	const size_t dll_fn_total_size = AlignUpToMultipleOf2(dll_fn_size, 4);

	const size_t func_name_size = strlen(func_name) + 1;
	const size_t func_name_total_size = AlignUpToMultipleOf2(func_name_size, 4);

	const size_t param_total_size = AlignUpToMultipleOf2(param_size, 4);

	const size_t grand_total_size = dll_dir_total_size + dll_fn_total_size + func_name_total_size + param_total_size + inject_size;

	// The workspace we will build the codecave on locally.
	uint8_t *const Buffer = (uint8_t *const)malloc(grand_total_size);

#define PatchInjectInst(inject_instance, inject_offset, type, value) \
{\
	type *const inject_instance_ptr = (type *const)((uintptr_t)(inject_instance) + (size_t)(inject_offset));\
	*inject_instance_ptr = (type)(value);\
}

	// Allocate space for the codecave in the process
	uint8_t *const CodecaveAddress = (uint8_t*)VirtualAllocEx(hProcess, 0, grand_total_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

	// Calculate an offset value to produce correct addresses
	// when writing the codecave to the other process.
	const size_t BufferOffset = CodecaveAddress - Buffer;

	uint8_t* workspace = Buffer;

	uint8_t* inject_ptr = (uint8_t*)memcpy(workspace, &inject, inject_size);
	workspace += inject_size;

	// Function Parameter
	uint8_t *const param_ptr = (uint8_t*)memcpy(workspace, param, param_size) + BufferOffset;
	workspace += param_total_size;
	PatchInjectInst(inject_ptr, inject_funcparam, const void*, param_ptr);

	// Directory name
	if (dll_dir) {
		uint8_t *const dll_dir_ptr = (uint8_t*)memcpy(workspace, dll_dir, dll_dir_size) + BufferOffset;
		workspace += dll_dir_total_size;
		PatchInjectInst(inject_ptr, inject_dlldir, const wchar_t*, dll_dir_ptr);
	}
	
	// Dll Name
	uint8_t *const dll_fn_ptr = (uint8_t*)memcpy(workspace, dll_fn, dll_fn_size) + BufferOffset;
	workspace += dll_fn_total_size;
	PatchInjectInst(inject_ptr, inject_dllname, const wchar_t*, dll_fn_ptr);

	// Function Name
	uint8_t *const func_name_ptr = (uint8_t *)memcpy(workspace, func_name, func_name_size) + BufferOffset;
	PatchInjectInst(inject_ptr, inject_funcname, const char*, func_name_ptr);

	PatchInjectInst(inject_ptr, inject_loadlibraryflags, size_t, PathIsRelativeW(dll_fn) || !have_kb2533623 ? 0 : LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);

	// Kernel functions will be located at
	// the same address in both processes.
	PatchInjectInst(inject_ptr, inject_GetCurrentDirectoryW, const void*, &GetCurrentDirectoryW);
	PatchInjectInst(inject_ptr, inject_SetCurrentDirectoryW, const void*, &SetCurrentDirectoryW);
	PatchInjectInst(inject_ptr, inject_LoadLibraryExW, const void*, &LoadLibraryExW);
	PatchInjectInst(inject_ptr, inject_ExitThread, const void*, &ExitThread);
	PatchInjectInst(inject_ptr, inject_GetProcAddress, const void*, &GetProcAddress);
	PatchInjectInst(inject_ptr, inject_FreeLibraryAndExitThread, const void*, &FreeLibraryAndExitThread);

	// Return code of injection function
	DWORD return_value;

	// Write out the patch, using return_value as a place to dump
	// an irrelevant value since it'll get overwritten later anyway.
	WriteProcessMemory(hProcess, CodecaveAddress, Buffer, grand_total_size, &return_value);

	// Free the workspace memory
	free(Buffer);

	// Make sure our changes are written right away
	FlushInstructionCache(hProcess, CodecaveAddress, grand_total_size);

	// Execute the thread now and wait for it to exit, note we execute where the code starts, and not the codecave start
	// (since we wrote strings at the start of the codecave)
	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)CodecaveAddress, 0, 0, NULL);
	WaitForSingleObject(hThread, INFINITE);

	// Cleanup and retrieve exit code
	GetExitCodeThread(hThread, &return_value);
	CloseHandle(hThread);

	// Free the memory in the process that we allocated
	VirtualFreeEx(hProcess, CodecaveAddress, 0, MEM_RELEASE);

	// 
	switch (return_value) {
		case 1: {
			static const wchar_t *const injectError1Format =
				L"Could not inject %ls.\n"
				"\n"
				"If you're running Windows Vista or 7, make sure that you have installed the KB2533623 update:\n"
				"\n"
				"\thttp://support.microsoft.com/kb/2533623/";
			const size_t injectError1_len = _scwprintf(injectError1Format, dll_fn) + 1;
			wchar_t *const injectError1 = (wchar_t*)malloc(injectError1_len);
			swprintf(injectError1, injectError1_len, injectError1Format, dll_fn);
			MessageBoxW(0, injectError1, L"Error", MB_ICONERROR);
			free(injectError1);
			break;
		}
		case 2: {
			static const char *const injectError2Format =
				"Could not load the function: %s";
			char *const injectError2 = (char*)malloc(snprintf(NULL, 0, injectError2Format, func_name) + 1);
			sprintf(injectError2, injectError2Format, func_name);
			MessageBoxA(0, injectError2, "Error", MB_ICONERROR);
			free(injectError2);
			break;
		}
	}
	return return_value;
}

int thcrap_inject_into_running(HANDLE hProcess, const char *run_cfg)
{
	int ret = -1;
	if (HMODULE inj_mod = GetModuleContaining(thcrap_inject_into_running)) {
		
		const size_t inj_dir_len = GetModuleFileNameU(inj_mod, NULL, 0) + 1;
		wchar_t* inj_dir = (wchar_t*)malloc(inj_dir_len * 2);
		GetModuleFileNameW(inj_mod, inj_dir, inj_dir_len);
		wchar_t* inj_dll = _wcsdup(inj_dir);
		PathRemoveFileSpecW(inj_dir);
		PathAddBackslashW(inj_dir);

		const size_t run_cfg_len = strlen(run_cfg) + 1;

		// Allow relative directory names
		if (*run_cfg != '{' && PathIsRelativeA(run_cfg)) {
			const size_t cur_dir_len = GetCurrentDirectoryA(0, NULL);
			const size_t total_size = run_cfg_len + cur_dir_len;
			char* param = (char*)malloc(total_size);
			GetCurrentDirectoryA(cur_dir_len, param);
			memcpy(param + cur_dir_len, run_cfg, run_cfg_len);
			ret = Inject(hProcess, inj_dir, inj_dll, "thcrap_init", param, total_size);
			free(param);
		}
		else {
			ret = Inject(hProcess, inj_dir, inj_dll, "thcrap_init", run_cfg, run_cfg_len);
		}
	}
	return ret;
}

BOOL thcrap_inject_into_new(const char *exe_fn, char *args, HANDLE *hProcess, HANDLE *hThread)
{
	int ret = 0;
	
	size_t exe_fn_len = strlen(exe_fn);
	char* exe_fn_local = strdup_size(exe_fn, exe_fn_len);
	str_slash_normalize_win(exe_fn_local);
	char* exe_dir = strdup_size(exe_fn_local, PtrDiffStrlen(strrchr(exe_fn_local, '\\') + 1, exe_fn_local));
	STARTUPINFOA si = {};
	PROCESS_INFORMATION pi = {};
	
	if (args) {
		size_t args_len = strlen(args);
		VLA(char, cmd, exe_fn_len + args_len + 4);
		cmd[0] = '\"';
		memcpy(cmd + 1, exe_fn_local, exe_fn_len);
		cmd[exe_fn_len + 1] = '\"';
		cmd[exe_fn_len + 2] = ' ';
		memcpy(cmd + exe_fn_len + 3, args, args_len);
		cmd[exe_fn_len + args_len + 3] = 0;

		ret = W32_ERR_WRAP(inject_CreateProcessU(
			NULL, cmd, NULL, NULL, TRUE, 0, NULL, exe_dir, &si, &pi
		));
		VLA_FREE(cmd);
	} else {
		ret = W32_ERR_WRAP(inject_CreateProcessU(
			exe_fn_local, NULL, NULL, NULL, TRUE, 0, NULL, exe_dir, &si, &pi
		));
	}

	/**
     * Sure, the alternative would be to set up the entire engine
     * with all plug-ins and modules to correctly run any additional
     * detours. While it would indeed be nice to allow those to control
     * initial startup, it really shouldn't be necessary for now - and
     * it really does run way too much unnecessary code for my taste.
     */

	if(ret) {
		char *msg_str = NULL;

		FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, ret, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&msg_str, 0, NULL
		);

		log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
			"Failed to start %s\n"
			"%s"
			, exe_fn_local, (msg_str ? msg_str : "")
		);
		LocalFree(msg_str);
	}
	free(exe_dir);
	free(exe_fn_local);

	if (hProcess) {
		*hProcess = pi.hProcess;
	}
	else {
		CloseHandle(pi.hProcess);
	}
	if (hThread) {
		*hThread = pi.hThread;
	}
	else {
		CloseHandle(pi.hThread);
	}
	return ret;
}

/// Entry point determination
/// -------------------------
void* entry_from_context(HANDLE hThread)
{
	CONTEXT context = {};
	context.ContextFlags = CONTEXT_INTEGER;
	if(GetThreadContext(hThread, &context)) {
#ifdef TH_X64
		return (void*)context.Rax;
#else
		return (void*)context.Eax;
#endif
	}
	return NULL;
}
/// -------------------------

int ThreadWaitUntil(HANDLE hProcess, HANDLE hThread, void *addr)
{
	CONTEXT context = {};
	BYTE entry_asm_orig[2];
	const BYTE entry_asm_delay[2] = {0xEB, 0xFE}; // JMP SHORT YADA YADA
	MEMORY_BASIC_INFORMATION mbi;
	SIZE_T byte_ret;
	DWORD old_prot;

	if(!VirtualQueryEx(hProcess, addr, &mbi, sizeof(mbi))) {
		return 1;
	}
	VirtualProtectEx(hProcess, mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &old_prot);
	ReadProcessMemory(hProcess, addr, entry_asm_orig, sizeof(entry_asm_orig), &byte_ret);
	WriteProcessMemory(hProcess, addr, entry_asm_delay, sizeof(entry_asm_delay), &byte_ret);
	FlushInstructionCache(hProcess, addr, sizeof(entry_asm_delay));

	while (true) {
		context.ContextFlags = CONTEXT_CONTROL;
		GetThreadContext(hThread, &context);
#ifdef TH_X64
		uintptr_t ip = context.Rip;
#else
		uintptr_t ip = context.Eip;
#endif
		if (ip != (uintptr_t)addr) {
			break ;
		}
		ResumeThread(hThread);
		Sleep(10);
		SuspendThread(hThread);
	}

	// Write back the original code
	WriteProcessMemory(hProcess, addr, entry_asm_orig, sizeof(entry_asm_orig), &byte_ret);
	FlushInstructionCache(hProcess, addr, sizeof(entry_asm_orig));

	VirtualProtectEx(hProcess, mbi.BaseAddress, mbi.RegionSize, old_prot, &old_prot);
	return 0;
}

int WaitUntilEntryPoint(HANDLE hProcess, HANDLE hThread, const char *module)
{
	// Try to get the entry point by various means, sorted by both efficiency
	// and probability of them working.
	void *entry_addr = NULL;

	/**
	  * Method 1: Initial value of EAX
	  * After creating a process in suspended state, EAX is guaranteed to contain
	  * the correct address of the entry point, even when the executable has the
	  * DYNAMICBASE flag activated in its header.
	  *
	  * (Works on Windows, but not on Wine)
	  */
	if(!(entry_addr = entry_from_context(hThread))) {
		HMODULE module_base;

		/**
		  * Method 2: EnumProcessModules, then parse the PE header.
		  *
		  * (Works on Wine, but not on Windows immediately after the target process
		  * was created in suspended state.)
		  */
		if(!(module_base = GetRemoteModuleHandle(hProcess, module))) {
			/**
			  * Method 3: Guessing 0x400000
			  * This is the default value in many compilers and should thus work for
			  * most non-ASLR Windows applications.
			  */
			module_base = (HMODULE)0x400000;
		}
		entry_addr = GetRemoteModuleEntryPoint(hProcess, module_base);
	}

	if(entry_addr) {
		return ThreadWaitUntil(hProcess, hThread, entry_addr);
	} else {
		log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
			"Couldn't determine the entry point of %s!\n"
			"\n"
			"Seems as if %s won't work with this game on your system.\n",
			PathFindFileNameU(module), PROJECT_NAME_SHORT
		);
		return 1;
	}
}

// Injection calls shared between the U and W versions
static inline void inject_CreateProcess_helper(
	LPPROCESS_INFORMATION lpPI,
	LPCSTR lpAppName,
	DWORD dwCreationFlags
)
{
	{
		std::wstring shared_mem_name = L"thcrap loader process list " + std::to_wstring(runconfig_loader_pid_get());
		std::wstring mutex_name = L"thcrap loader process list mutex " + std::to_wstring(runconfig_loader_pid_get());

		HANDLE hFileMapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, shared_mem_name.c_str());
		HANDLE hMutex = OpenMutexW(SYNCHRONIZE, FALSE, mutex_name.c_str());

		if (hMutex) {
			DWORD* pid_list = (DWORD*)MapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 128 * sizeof(DWORD));

			WaitForSingleObject(hMutex, INFINITE);

			for (size_t i = 0; i < 128; i++) {
				if (!pid_list[i]) {
					pid_list[i] = lpPI->dwProcessId;
					break;
				}
			}

			ReleaseMutex(hMutex);

			CloseHandle(hFileMapping);
			CloseHandle(hMutex);
		}
	}

	if (GetProcessId(GetCurrentProcess()) != runconfig_loader_pid_get()) {
		json_t* versions_js = stack_json_resolve("versions.js", NULL);
		size_t exe_size;
		game_version* id = identify_by_hash(lpAppName, &exe_size, versions_js);
		if (!id) id = identify_by_size(exe_size, versions_js);

		if (id && id->id) {
			std::wstring mailslotName = L"\\\\.\\mailslot\\thcrap_request_update_" + std::to_wstring(runconfig_loader_pid_get());
			HANDLE hMail = CreateFileW(mailslotName.c_str(), GENERIC_WRITE, 1, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hMail != INVALID_HANDLE_VALUE) {
				std::string event_name = "thcrap inject " + std::to_string(GetProcessId(GetCurrentProcess()));

				json_t* data_json = json_object();
				json_object_set(data_json, "event_name", json_string(event_name.c_str()));
				json_object_set(data_json, "game_id", json_string(id->id));

				char* data_to_send = json_dumps(data_json, 0);

				json_decref(data_json);

				size_t data_to_send_len = strlen(data_to_send);

				if (data_to_send_len < 1024) {
					const wchar_t* event_name_w = (wchar_t*)utf8_to_utf16(event_name.c_str());
					HANDLE hEvent = CreateEventW(NULL, FALSE, FALSE, event_name_w);
					free((void*)event_name_w);
					DWORD byteRet;
					WriteFile(hMail, data_to_send, data_to_send_len, &byteRet, NULL);

					HANDLE hLoaderProcess = OpenProcess(SYNCHRONIZE, FALSE, runconfig_loader_pid_get());

					if (hLoaderProcess) {
						HANDLE handles[] = {
							hEvent,
							hLoaderProcess
						};
						WaitForMultipleObjects(elementsof(handles), handles, FALSE, INFINITE);
						CloseHandle(hLoaderProcess);
					}
					CloseHandle(hEvent);
				}
				free((void*)data_to_send);
				CloseHandle(hMail);
			}
		}
	}

	if(!WaitUntilEntryPoint(lpPI->hProcess, lpPI->hThread, lpAppName)) {
		char *run_cfg = json_dumps(runconfig_json_get(), JSON_COMPACT);

		thcrap_inject_into_running(lpPI->hProcess, run_cfg);

		free(run_cfg);
	}
	if(~dwCreationFlags & CREATE_SUSPENDED) {
		ResumeThread(lpPI->hThread);
	}
}

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
)
{
	BOOL ret = chain_CreateProcessU(
		lpAppName, lpCmdLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles,
		dwCreationFlags | CREATE_SUSPENDED,
		lpEnvironment, lpCurrentDirectory, lpSI, lpPI
	);
	if(ret) {
		inject_CreateProcess_helper(lpPI, lpAppName, dwCreationFlags);
	}
	return ret;
}

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
)
{
	BOOL ret = chain_CreateProcessW(
		lpAppName, lpCmdLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles,
		dwCreationFlags | CREATE_SUSPENDED,
		lpEnvironment, lpCurrentDirectory, lpSI, lpPI
	);
	if(ret) {
		UTF8_DEC(lpAppName);
		UTF8_CONV(lpAppName);
		inject_CreateProcess_helper(lpPI, lpAppName_utf8, dwCreationFlags);
		UTF8_FREE(lpAppName);
	}
	return ret;
}

LPTHREAD_START_ROUTINE inject_change_start_func(
	HANDLE hProcess,
	LPTHREAD_START_ROUTINE lpStartAddress,
	FARPROC rep_func,
	const char *new_dll,
	const char *new_func
)
{
	LPTHREAD_START_ROUTINE ret = NULL;
	if((FARPROC)lpStartAddress == rep_func) {
		HMODULE hMod = GetRemoteModuleHandle(hProcess, new_dll);
		ret = (LPTHREAD_START_ROUTINE)GetRemoteProcAddress(hProcess, hMod, new_func);
	}
	return ret ? ret : lpStartAddress;
}

HANDLE WINAPI inject_CreateRemoteThread(
	HANDLE hProcess,
	LPSECURITY_ATTRIBUTES lpThreadAttributes,
	SIZE_T dwStackSize,
	LPTHREAD_START_ROUTINE lpFunc,
	LPVOID lpParameter,
	DWORD dwCreationFlags,
	LPDWORD lpThreadId
)
{
	const char *thcrap_dll = "thcrap" DEBUG_OR_RELEASE ".dll";
	HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
	FARPROC kernel32_LoadLibraryA = GetProcAddress(hKernel32, "LoadLibraryA");
	FARPROC kernel32_LoadLibraryW = GetProcAddress(hKernel32, "LoadLibraryW");

	lpFunc = inject_change_start_func(
		hProcess, lpFunc, kernel32_LoadLibraryA, thcrap_dll, "inject_LoadLibraryU"
	);
	lpFunc = inject_change_start_func(
		hProcess, lpFunc, kernel32_LoadLibraryW, thcrap_dll, "inject_LoadLibraryW"
	);

	return (HANDLE)chain_CreateRemoteThread(
		hProcess, lpThreadAttributes, dwStackSize, lpFunc,
		lpParameter, dwCreationFlags, lpThreadId
	);
}

HMODULE WINAPI inject_LoadLibraryU(
	LPCSTR lpLibFileName
)
{
	auto previous_mod = GetModuleHandleA(lpLibFileName);
	HMODULE ret = chain_LoadLibraryU(lpLibFileName);
	if(ret && previous_mod != ret) {
		thcrap_detour(ret);
	}
	return ret;
}

HMODULE WINAPI inject_LoadLibraryW(
	LPCWSTR lpLibFileName
)
{
	auto previous_mod = GetModuleHandleW(lpLibFileName);
	HMODULE ret = (HMODULE)chain_LoadLibraryW(lpLibFileName);
	if(ret && previous_mod != ret) {
		thcrap_detour(ret);
	}
	return ret;
}

void inject_mod_detour(void)
{
	detour_chain("kernel32.dll", 1,
		"CreateProcessA", inject_CreateProcessU, &chain_CreateProcessU,
		"CreateProcessW", inject_CreateProcessW, &chain_CreateProcessW,
		"CreateRemoteThread", inject_CreateRemoteThread, &chain_CreateRemoteThread,
		"LoadLibraryA", inject_LoadLibraryU, &chain_LoadLibraryU,
		"LoadLibraryW", inject_LoadLibraryW, &chain_LoadLibraryW,
		NULL
	);
}
