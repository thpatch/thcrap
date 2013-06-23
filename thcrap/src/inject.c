/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * DLL injector.
  * Adapted from http://www.codeproject.com/Articles/20084/completeinject
  */

#include "thcrap.h"

/**
  * A more complete DLL injection solution.
  *
  * Parameters
  * ----------
  *	HANDLE hProcess
  *		The handle to the process to inject the DLL into.
  *
  *	const wchar_t *dll_dir
  *		Directory of the DLL to inject. (optional)
  *		During injection, the target application's directory can be temporarily
  *		switched to this directory. This may be necessary if the injected DLL
  *		depends on other DLLs stored in the same directory.
  *
  *	const wchar_t *dll_name
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
  *		__declspec(dllexport) void FunctionName(void*)
  *
  * The function that is called in the injected DLL -MUST- return, the loader
  * waits for the thread to terminate before removing the allocated space and 
  * eturning control to the Loader. This method of DLL injection also adds error
  * handling, so the end user knows if something went wrong.
  */

int Inject(HANDLE hProcess, const wchar_t *dll_dir, const wchar_t *dll_name, const char *func_name, const void *param, const size_t param_size)
{
//------------------------------------------//
// Function variables.                      //
//------------------------------------------//

	// Main DLL we will need to load
	HMODULE kernel32 = NULL;

	// Main functions we will need to import
	FARPROC getcurrentdirectory = NULL;
	FARPROC setcurrentdirectory = NULL;
	FARPROC loadlibrary = NULL;
	FARPROC getprocaddress = NULL;
	FARPROC exitprocess = NULL;
	FARPROC exitthread	 = NULL;
	FARPROC freelibraryandexitthread = NULL;

	// The workspace we will build the codecave on locally
	LPBYTE workspace = NULL;
	DWORD workspaceIndex = 0;
	const DWORD workspaceSize = 2048;

	// The memory in the process we write to
	LPVOID codecaveAddress	= NULL;
	DWORD dwCodecaveAddress = 0;

	// Strings we have to write into the process
	wchar_t *injectError0 = L"Error";
	wchar_t injectError1[MAX_PATH + 1] = {0};
	char injectError2A[MAX_PATH + 1] = {0};
	wchar_t injectError2[MAX_PATH + 1] = {0};
	wchar_t *user32Name = L"user32.dll";
	char *msgboxName = "MessageBoxW";

	// Placeholder addresses to use the strings
	DWORD user32NameAddr	= 0;
	DWORD user32Addr		= 0;
	DWORD msgboxNameAddr	= 0;
	DWORD msgboxAddr		= 0;
	DWORD dllAddr			= 0;
	DWORD dllDirAddr      = 0;
	DWORD dllNameAddr		= 0;
	DWORD funcNameAddr		= 0;
	DWORD funcParamAddr	= 0;
	DWORD error0Addr		= 0;
	DWORD error1Addr		= 0;
	DWORD error2Addr		= 0;

	// Where the codecave execution should begin at
	DWORD codecaveExecAddr = 0;

	// Handle to the thread we create in the process
	HANDLE hThread = NULL;

	// Temp variables
	DWORD dwTmpSize = 0;

	// Old protection on page we are writing to in the process and the bytes written
	DWORD oldProtect = 0;	
	DWORD byte_ret = 0;

	// Return code of injection function
	DWORD injRet;

//------------------------------------------//
// Variable initialization.                 //
//------------------------------------------//

	// Get the address of the main DLL
	kernel32	= GetModuleHandleA("kernel32.dll");

	// Get our functions
	getcurrentdirectory = GetProcAddress(kernel32,	"GetCurrentDirectoryW");
	setcurrentdirectory = GetProcAddress(kernel32,	"SetCurrentDirectoryW");
	loadlibrary = GetProcAddress(kernel32, "LoadLibraryW");
	getprocaddress = GetProcAddress(kernel32, "GetProcAddress");
	exitprocess = GetProcAddress(kernel32, "ExitProcess");
	exitthread = GetProcAddress(kernel32, "ExitThread");
	freelibraryandexitthread = GetProcAddress(kernel32, "FreeLibraryAndExitThread");

// This section will cause compiler warnings on VS8, 
// you can upgrade the functions or ignore them

	// Build error messages
	wcscpy(injectError1, L"Could not load the dll: ");
	wcscat(injectError1, dll_name);
	sprintf(injectError2A, "Could not load the function: %s", func_name);
	StringToUTF16(injectError2, injectError2A, MAX_PATH + 1);

	// Create the workspace
	workspace = (LPBYTE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, workspaceSize);

	// Allocate space for the codecave in the process
	codecaveAddress = VirtualAllocEx(hProcess, 0, workspaceSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	dwCodecaveAddress = PtrToUlong(codecaveAddress);

// Note there is no error checking done above for any functions that return a pointer/handle.
// I could have added them, but it'd just add more messiness to the code and not provide any real
// benefit. It's up to you though in your final code if you want it there or not.

//------------------------------------------//
// Data and string writing.                 //
//------------------------------------------//

	// Write out the address for the user32 dll address
	user32Addr = workspaceIndex + dwCodecaveAddress;
	dwTmpSize = 0;
	memcpy(workspace + workspaceIndex, &dwTmpSize, 4);
	workspaceIndex += 4;

	// Write out the address for the MessageBoxW address
	msgboxAddr = workspaceIndex + dwCodecaveAddress;
	dwTmpSize = 0;
	memcpy(workspace + workspaceIndex, &dwTmpSize, 4);
	workspaceIndex += 4;

	// Write out the address for the injected DLL's module
	dllAddr = workspaceIndex + dwCodecaveAddress;
	dwTmpSize = 0;
	memcpy(workspace + workspaceIndex, &dwTmpSize, 4);
	workspaceIndex += 4;

	// User32 Dll Name
	user32NameAddr = workspaceIndex + dwCodecaveAddress;
	dwTmpSize = (DWORD)(wcslen(user32Name) + 1) * 2;
	memcpy(workspace + workspaceIndex, user32Name, dwTmpSize);
	workspaceIndex += dwTmpSize;

	// MessageBoxW name
	msgboxNameAddr = workspaceIndex + dwCodecaveAddress;
	dwTmpSize = (DWORD)strlen(msgboxName) + 1;
	memcpy(workspace + workspaceIndex, msgboxName, dwTmpSize);
	workspaceIndex += dwTmpSize;

	// Directory name
	if(dll_dir) {
		dllDirAddr = workspaceIndex + dwCodecaveAddress;
		dwTmpSize = (wcslen(dll_dir) + 1) * sizeof(wchar_t);
		memcpy(workspace + workspaceIndex, dll_dir, dwTmpSize);
		workspaceIndex += dwTmpSize;
	}

	// Dll Name
	dllNameAddr = workspaceIndex + dwCodecaveAddress;
	dwTmpSize = (wcslen(dll_name) + 1) * sizeof(wchar_t);
	memcpy(workspace + workspaceIndex, dll_name, dwTmpSize);
	workspaceIndex += dwTmpSize;

	// Function Name
	funcNameAddr = workspaceIndex + dwCodecaveAddress;
	dwTmpSize = strlen(func_name) + 1;
	memcpy(workspace + workspaceIndex, func_name, dwTmpSize);
	workspaceIndex += dwTmpSize;

	// Function Parameter
	funcParamAddr = workspaceIndex + dwCodecaveAddress;
	dwTmpSize = param_size;
	memcpy(workspace + workspaceIndex, param, param_size);
	workspaceIndex += dwTmpSize;

	// Error Message 1
	error0Addr = workspaceIndex + dwCodecaveAddress;
	dwTmpSize = (wcslen(injectError0) + 1) * sizeof(wchar_t);
	memcpy(workspace + workspaceIndex, injectError0, dwTmpSize);
	workspaceIndex += dwTmpSize;

	// Error Message 2
	error1Addr = workspaceIndex + dwCodecaveAddress;
	dwTmpSize = (wcslen(injectError1) + 1) * sizeof(wchar_t);
	memcpy(workspace + workspaceIndex, injectError1, dwTmpSize);
	workspaceIndex += dwTmpSize;

	// Error Message 3
	error2Addr = workspaceIndex + dwCodecaveAddress;
	dwTmpSize = (DWORD)(wcslen(injectError2) + 1) * 2;
	memcpy(workspace + workspaceIndex, injectError2, dwTmpSize);
	workspaceIndex += dwTmpSize;

	// Pad a few INT3s after string data is written for seperation
	workspace[workspaceIndex++] = 0xCC;
	workspace[workspaceIndex++] = 0xCC;
	workspace[workspaceIndex++] = 0xCC;

	// Store where the codecave execution should begin
	codecaveExecAddr = workspaceIndex + dwCodecaveAddress;

// For debugging - infinite loop, attach onto process and step over
	//workspace[workspaceIndex++] = 0xEB;
	//workspace[workspaceIndex++] = 0xFE;

//------------------------------------------//
// User32.dll loading.                      //
//------------------------------------------//

// User32 DLL Loading
	// PUSH 0x00000000 - Push the address of the DLL name to use in LoadLibraryA
	workspace[workspaceIndex++] = 0x68;
	memcpy(workspace + workspaceIndex, &user32NameAddr, 4);
	workspaceIndex += 4;

	// MOV EAX, ADDRESS - Move the address of LoadLibraryA into EAX
	workspace[workspaceIndex++] = 0xB8;
	memcpy(workspace + workspaceIndex, &loadlibrary, 4);
	workspaceIndex += 4;

	// CALL EAX - Call LoadLibraryA
	workspace[workspaceIndex++] = 0xFF;
	workspace[workspaceIndex++] = 0xD0;

// MessageBoxW Loading
	// PUSH 0x000000 - Push the address of the function name to load
	workspace[workspaceIndex++] = 0x68;
	memcpy(workspace + workspaceIndex, &msgboxNameAddr, 4);
	workspaceIndex += 4;

	// Push EAX, module to use in GetProcAddress
	workspace[workspaceIndex++] = 0x50;

	// MOV EAX, ADDRESS - Move the address of GetProcAddress into EAX
	workspace[workspaceIndex++] = 0xB8;
	memcpy(workspace + workspaceIndex, &getprocaddress, 4);
	workspaceIndex += 4;

	// CALL EAX - Call GetProcAddress
	workspace[workspaceIndex++] = 0xFF;
	workspace[workspaceIndex++] = 0xD0;

	// MOV [ADDRESS], EAX - Save the address to our variable
	workspace[workspaceIndex++] = 0xA3;
	memcpy(workspace + workspaceIndex, &msgboxAddr, 4);
	workspaceIndex += 4;

//------------------------------------------//
// Injected dll loading.                    //
//------------------------------------------//

/*
	// This is the way the following assembly code would look like in C/C++

	// In case the injected DLL depends on other DLLs,
	// we need to change the current directory to the one given as parameter
	if(dll_dir) {
		size_t cur_dir_len = GetCurrentDirectory(0, NULL) + 1;
		VLA(wchar_t, cur_dir, cur_dir_len);
		GetCurrentDirectory(cur_dir, cur_dir_len);
		SetCurrentDirectory(dll_dir);
	}

	// Load the injected DLL into this process
	HMODULE h = LoadLibrary(dll_name);
	if(!h) {
		MessageBox(0, "Could not load the dll: mydll.dll", "Error", MB_ICONERROR);
		ExitProcess(0);
	}

	if(dll_dir) {
		SetCurrentDirectory(cur_dir);
	}

	// Get the address of the export function
	FARPROC p = GetProcAddress(h, func_name);
	if(!p) {
		MessageBox(0, "Could not load the function: func_name", "Error", MB_ICONERROR);
		ExitProcess(0);
	}

	// So we do not need a function pointer interface
	__asm call p

	// Exit the thread so the loader continues
	ExitThread(0);
*/

// DLL Loading

	if(dllDirAddr) {
		// Registers:

		// ebp: Base stack frame
		// esi: GetCurrentDirectory / SetCurrentDirectory
		// ebx: Current directory of process (on stack)
		// ecx: byte length of string at ebx

		// mov ebp, esp - Save stack frame
		workspace[workspaceIndex++] = 0x89;
		workspace[workspaceIndex++] = 0xe5;

		// Get length for current directory

		// push 0
		// push 0
		workspace[workspaceIndex++] = 0x6a;
		workspace[workspaceIndex++] = 0x00;
		workspace[workspaceIndex++] = 0x6a;
		workspace[workspaceIndex++] = 0x00;
		// mov esi, GetCurrentDirectory
		workspace[workspaceIndex++] = 0xbe;
		memcpy(workspace + workspaceIndex, &getcurrentdirectory, 4);
		workspaceIndex += 4;

		// call esi
		workspace[workspaceIndex++] = 0xFF;
		workspace[workspaceIndex++] = 0xD6;

		/// Calculate byte size of directory buffer.
		/// Also do some poor man's DWORD boundary alignment
		/// in order to not fuck up the stack

		// mov ecx, eax
		// shl ecx, 1
		// and ecx, fffffff8
		// add ecx, 4

		workspace[workspaceIndex++] = 0x89;
		workspace[workspaceIndex++] = 0xc1;
		workspace[workspaceIndex++] = 0xd1;
		workspace[workspaceIndex++] = 0xe1;
		workspace[workspaceIndex++] = 0x83;
		workspace[workspaceIndex++] = 0xe1;
		workspace[workspaceIndex++] = 0xf8;
		workspace[workspaceIndex++] = 0x83;
		workspace[workspaceIndex++] = 0xc1;
		workspace[workspaceIndex++] = 0x04;
		
		/// "Allocate" ecx bytes on stack and store buffer pointer to ebx

		// sub esp, ecx
		// mov ebx, esp

		workspace[workspaceIndex++] = 0x29;
		workspace[workspaceIndex++] = 0xcc;
		workspace[workspaceIndex++] = 0x89;
		workspace[workspaceIndex++] = 0xe3;

		/// Call GetCurrentDirectory
		// push ebx
		// push eax
		// call esi
		workspace[workspaceIndex++] = 0x53;
		workspace[workspaceIndex++] = 0x50;
		workspace[workspaceIndex++] = 0xff;
		workspace[workspaceIndex++] = 0xd6;

		/// PUSH 0x00000000 - Push the address of our directory
		workspace[workspaceIndex++] = 0x68;
		memcpy(workspace + workspaceIndex, &dllDirAddr, 4);
		workspaceIndex += 4;

		// mov esi, SetCurrentDirectory
		workspace[workspaceIndex++] = 0xbe;
		memcpy(workspace + workspaceIndex, &setcurrentdirectory, 4);
		workspaceIndex += 4;

		// call esi
		workspace[workspaceIndex++] = 0xFF;
		workspace[workspaceIndex++] = 0xD6;
	}

	// PUSH 0x00000000 - Push the address of the DLL name to use in LoadLibraryA
	workspace[workspaceIndex++] = 0x68;
	memcpy(workspace + workspaceIndex, &dllNameAddr, 4);
	workspaceIndex += 4;

	// MOV EAX, ADDRESS - Move the address of LoadLibrary into EAX
	workspace[workspaceIndex++] = 0xB8;
	memcpy(workspace + workspaceIndex, &loadlibrary, 4);
	workspaceIndex += 4;
	
	// CALL EAX - Call LoadLibrary
	workspace[workspaceIndex++] = 0xFF;
	workspace[workspaceIndex++] = 0xD0;

	// mov edi, eax - Save return value
	workspace[workspaceIndex++] = 0x89;
	workspace[workspaceIndex++] = 0xc7;

	if(dllDirAddr) {
		/// Reset directory to the original one of the process
		// push ebx
		// call esi
		workspace[workspaceIndex++] = 0x53;
		workspace[workspaceIndex++] = 0xFF;
		workspace[workspaceIndex++] = 0xD6;

		/// Reset stack frame
		// mov esp, ebp
		workspace[workspaceIndex++] = 0x89;
		workspace[workspaceIndex++] = 0xec;
	}

// Error Checking
	// CMP EDI, 0
	workspace[workspaceIndex++] = 0x83;
	workspace[workspaceIndex++] = 0xFF;
	workspace[workspaceIndex++] = 0x00;

// JNZ EIP + 0x1E to skip over eror code
	workspace[workspaceIndex++] = 0x75;
	workspace[workspaceIndex++] = 0x1E;

// Error Code 1
	// MessageBox
		// PUSH 0x10 (MB_ICONHAND)
		workspace[workspaceIndex++] = 0x6A;
		workspace[workspaceIndex++] = 0x10;

		// PUSH 0x000000 - Push the address of the MessageBox title
		workspace[workspaceIndex++] = 0x68;
		memcpy(workspace + workspaceIndex, &error0Addr, 4);
		workspaceIndex += 4;

		// PUSH 0x000000 - Push the address of the MessageBox message
		workspace[workspaceIndex++] = 0x68;
		memcpy(workspace + workspaceIndex, &error1Addr, 4);
		workspaceIndex += 4;

		// Push 0
		workspace[workspaceIndex++] = 0x6A;
		workspace[workspaceIndex++] = 0x00;

		// MOV EAX, [ADDRESS] - Move the address of MessageBoxW into EAX
		workspace[workspaceIndex++] = 0xA1;
		memcpy(workspace + workspaceIndex, &msgboxAddr, 4);
		workspaceIndex += 4;

		// CALL EAX - Call MessageBoxW
		workspace[workspaceIndex++] = 0xFF;
		workspace[workspaceIndex++] = 0xD0;

	// ExitThread
		// PUSH 1
		workspace[workspaceIndex++] = 0x6A;
		workspace[workspaceIndex++] = 0x01;

		// MOV EAX, ADDRESS - Move the address of ExitThread into EAX
		workspace[workspaceIndex++] = 0xB8;
		memcpy(workspace + workspaceIndex, &exitthread, 4);
		workspaceIndex += 4;

		// CALL EAX - Call ExitThread
		workspace[workspaceIndex++] = 0xFF;
		workspace[workspaceIndex++] = 0xD0;

//	Now we have the address of the injected DLL, so save the handle

	// MOV [ADDRESS], EAX - Save the address to our variable
	workspace[workspaceIndex++] = 0x89;
	workspace[workspaceIndex++] = 0x3D;
	memcpy(workspace + workspaceIndex, &dllAddr, 4);
	workspaceIndex += 4;

// Load the initilize function from it

	// PUSH 0x000000 - Push the address of the function name to load
	workspace[workspaceIndex++] = 0x68;
	memcpy(workspace + workspaceIndex, &funcNameAddr, 4);
	workspaceIndex += 4;

	// Push EDI - module to use in GetProcAddress
	workspace[workspaceIndex++] = 0x57;

	// MOV EAX, ADDRESS - Move the address of GetProcAddress into EAX
	workspace[workspaceIndex++] = 0xB8;
	memcpy(workspace + workspaceIndex, &getprocaddress, 4);
	workspaceIndex += 4;

	// CALL EAX - Call GetProcAddress
	workspace[workspaceIndex++] = 0xFF;
	workspace[workspaceIndex++] = 0xD0;

// Error Checking
	// CMP EAX, 0
	workspace[workspaceIndex++] = 0x83;
	workspace[workspaceIndex++] = 0xF8;
	workspace[workspaceIndex++] = 0x00;

// JNZ EIP + 0x23 to skip eror code
	workspace[workspaceIndex++] = 0x75;
	workspace[workspaceIndex++] = 0x23;

// Error Code 2
	// MessageBox
		// PUSH 0x10 (MB_ICONHAND)
		workspace[workspaceIndex++] = 0x6A;
		workspace[workspaceIndex++] = 0x10;

		// PUSH 0x000000 - Push the address of the MessageBox title
		workspace[workspaceIndex++] = 0x68;
		memcpy(workspace + workspaceIndex, &error0Addr, 4);
		workspaceIndex += 4;

		// PUSH 0x000000 - Push the address of the MessageBox message
		workspace[workspaceIndex++] = 0x68;
		memcpy(workspace + workspaceIndex, &error2Addr, 4);
		workspaceIndex += 4;

		// Push 0
		workspace[workspaceIndex++] = 0x6A;
		workspace[workspaceIndex++] = 0x00;

		// MOV EAX, ADDRESS - Move the address of MessageBoxA into EAX
		workspace[workspaceIndex++] = 0xA1;
		memcpy(workspace + workspaceIndex, &msgboxAddr, 4);
		workspaceIndex += 4;

		// CALL EAX - Call MessageBoxA
		workspace[workspaceIndex++] = 0xFF;
		workspace[workspaceIndex++] = 0xD0;

	// FreeLibraryAndExitThread
		// PUSH 2
		workspace[workspaceIndex++] = 0x6A;
		workspace[workspaceIndex++] = 0x02;

		// PUSH 0x000000 - Push the injected DLL's module handle
		workspace[workspaceIndex++] = 0x68;
		memcpy(workspace + workspaceIndex, &dllAddr, 4);
		workspaceIndex += 4;

		// MOV EAX, ADDRESS - Move the address of FreeLibraryAndExitThread into EAX
		workspace[workspaceIndex++] = 0xB8;
		memcpy(workspace + workspaceIndex, &freelibraryandexitthread, 4);
		workspaceIndex += 4;

		// CALL EAX - Call ExitThread function
		workspace[workspaceIndex++] = 0xFF;
		workspace[workspaceIndex++] = 0xD0;

	// PUSH 0x000000 - Push the address of the function parameter
	workspace[workspaceIndex++] = 0x68;
	memcpy(workspace + workspaceIndex, &funcParamAddr, 4);
	workspaceIndex += 4;
	
	// CALL EAX - Call the Initialize function
	workspace[workspaceIndex++] = 0xFF;
	workspace[workspaceIndex++] = 0xD0;

	// If we get here, the Initialize function has been called, 
	// so it's time to close this thread and optionally unload the DLL.

//------------------------------------------//
// Exiting from the injected dll.           //
//------------------------------------------//

// Call ExitThread to leave the DLL loaded
#if 1
	// PUSH 0 (exit code)
	workspace[workspaceIndex++] = 0x6A;
	workspace[workspaceIndex++] = 0x00;

	// MOV EAX, ADDRESS - Move the address of ExitThread into EAX
	workspace[workspaceIndex++] = 0xB8;
	memcpy(workspace + workspaceIndex, &exitthread, 4);
	workspaceIndex += 4;

	// CALL EAX - Call ExitThread
	workspace[workspaceIndex++] = 0xFF;
	workspace[workspaceIndex++] = 0xD0;
#endif

// Call FreeLibraryAndExitThread to unload DLL
#if 0
	// Push 0 (exit code)
	workspace[workspaceIndex++] = 0x6A;
	workspace[workspaceIndex++] = 0x00;

	// PUSH [0x000000] - Push the address of the DLL module to unload
	workspace[workspaceIndex++] = 0xFF;
	workspace[workspaceIndex++] = 0x35;
	memcpy(workspace + workspaceIndex, &dllAddr, 4);
	workspaceIndex += 4;

	// MOV EAX, ADDRESS - Move the address of FreeLibraryAndExitThread into EAX
	workspace[workspaceIndex++] = 0xB8;
	memcpy(workspace + workspaceIndex, &freelibraryandexitthread, 4);
	workspaceIndex += 4;

	// CALL EAX - Call FreeLibraryAndExitThread
	workspace[workspaceIndex++] = 0xFF;
	workspace[workspaceIndex++] = 0xD0;
#endif

//------------------------------------------//
// Code injection and cleanup.              //
//------------------------------------------//

	// Change page protection so we can write executable code
	VirtualProtectEx(hProcess, codecaveAddress, workspaceIndex, PAGE_EXECUTE_READWRITE, &oldProtect);

	// Write out the patch
	WriteProcessMemory(hProcess, codecaveAddress, workspace, workspaceIndex, &byte_ret);

	// Restore page protection
	VirtualProtectEx(hProcess, codecaveAddress, workspaceIndex, oldProtect, &oldProtect);

	// Make sure our changes are written right away
	FlushInstructionCache(hProcess, codecaveAddress, workspaceIndex);

	// Free the workspace memory
	HeapFree(GetProcessHeap(), 0, workspace);

	// Execute the thread now and wait for it to exit, note we execute where the code starts, and not the codecave start
	// (since we wrote strings at the start of the codecave) -- NOTE: void* used for VC6 compatibility instead of UlongToPtr
	hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)((void*)codecaveExecAddr), 0, 0, NULL);
	WaitForSingleObject(hThread, INFINITE);

	GetExitCodeThread(hThread, &injRet);
	CloseHandle(hThread);

	// Free the memory in the process that we allocated
	VirtualFreeEx(hProcess, codecaveAddress, 0, MEM_RELEASE);

	return injRet;
}

// Injects thcrap into the given [hProcess], and passes [setup_fn] as parameter.
DWORD thcrap_inject(HANDLE hProcess, const char *setup_fn)
{
#ifdef _DEBUG
	const wchar_t *inj_dll = L"thcrap_d.dll";
#else
	const wchar_t *inj_dll = L"thcrap.dll";
#endif

	size_t setup_fn_len;
	DWORD cur_dir_len = GetCurrentDirectoryW(0, NULL) + 1;
	VLA(wchar_t, cur_dir, cur_dir_len);

	// Yeah, GetModuleFileName() sucks.
	wchar_t prog_dir[MAX_PATH * 4];
	
	const wchar_t *inj_dir;

	GetModuleFileNameW(NULL, prog_dir, MAX_PATH);
	GetCurrentDirectoryW(cur_dir_len, cur_dir);

	if(prog_dir) {
		PathRemoveFileSpecW(prog_dir);
		PathAddBackslash(prog_dir);
		inj_dir = prog_dir;
	} else {
		inj_dir = cur_dir;
	}
	setup_fn_len = strlen(setup_fn) + 1;

	{
		DWORD ret;
		size_t full_setup_fn_len = (cur_dir_len * UTF8_MUL) + 1 + setup_fn_len + 1;
		VLA(char, abs_setup_fn, full_setup_fn_len);
		const char *full_setup_fn;

		// Account for relative directory names
		if(PathIsRelativeA(setup_fn)) {
			StringToUTF8(abs_setup_fn, cur_dir, cur_dir_len);
			PathAppendA(abs_setup_fn, setup_fn);
			full_setup_fn = abs_setup_fn;
		} else {
			full_setup_fn = setup_fn;
			full_setup_fn_len = setup_fn_len;
		}
		ret = Inject(hProcess, inj_dir, inj_dll, "thcrap_init", full_setup_fn, full_setup_fn_len);
		VLA_FREE(cur_dir);
		VLA_FREE(abs_setup_fn);
		return ret;
	}
}
