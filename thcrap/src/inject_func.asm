/*
	//------------------------------------------//
	// Injected dll loading.                    //
	//------------------------------------------//
	
	This is how following assembly code would look like in C/C++

	// In case the injected DLL depends on other DLLs,
	// we need to change the current directory to the one given as parameter
	wchar_t* dll_dir_ptr = dll_dir;
	if(dll_dir_ptr) {
		size_t cur_dir_len = GetCurrentDirectoryW(0, NULL) + 1;
		VLA(wchar_t, cur_dir, cur_dir_len);
		GetCurrentDirectoryW(cur_dir, cur_dir_len);
		SetCurrentDirectoryW(dll_dir);
	}

	// Load the injected DLL into this process
	HMODULE h = LoadLibraryEx(dll_fn, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
	if(!h) {
		ExitThread(1);
	}

	if(dll_dir) {
		SetCurrentDirectory(cur_dir);
	}

	// Get the address of the export function
	void (*p)(void*) = GetProcAddress(h, func_name);
	if(!p) {
		FreeLibraryAndExitThread(h, 2);
	}

	p(param);

	// Exit the thread so the loader continues
	ExitThread(0);
*/

	.intel_syntax noprefix
	.global _inject, _inject_dlldirptr, _inject_loadlibraryflagsptr, _inject_dllnameptr, _inject_funcnameptr, _inject_funcparamptr, _inject_end

	/*
	  Note: All of these functions use __stdcall, thus
	  cleaning their own parameters off the stack.
	*/
	.global _inject_GetCurrentDirectoryWptr, _inject_SetCurrentDirectoryWptr, _inject_LoadLibraryExWptr, _inject_ExitThreadptr, _inject_GetProcAddressptr, _inject_FreeLibraryAndExitThreadptr

	.macro pushDW value
	.byte 0x68
	.int \value
	.endm

_inject:
	push	ebx
	push	ebp
	push	esi
	push	edi
_inject_dlldirptr:
	mov		ebp, 0x00000000
	test	ebp, ebp
	jz		SkipDirectoryBS1

	/* Get length for current directory */

	push	0
	push	0
_inject_GetCurrentDirectoryWptr:
	mov		esi, 0xDEADBEEF
	call	esi /* Call GetCurrentDirectoryW */
	lea		ebx, [eax + eax + 5] /* Calculate byte size of directory buffer. */
	and		ebx, 0xFFFFFFFC /* Also do some DWORD boundary alignment. */
	sub		esp, ebx /* Allocate a suitable buffer on stack. */
	push	esp
	push	eax
	call	esi /* Call GetCurrentDirectoryW */
	push	ebp /* Push the address of our directory */
_inject_SetCurrentDirectoryWptr:
	mov		esi, 0xDEADBEEF
	call	esi /* Call SetCurrentDirectoryW */
SkipDirectoryBS1:
_inject_loadlibraryflagsptr:
	pushDW	0x00000000
	push	0
_inject_dllnameptr:
	push	0xDEADBEEF /* Push the address of the DLL name to use in LoadLibraryEx */
_inject_LoadLibraryExWptr:
	mov		eax, 0xDEADBEEF
	call	eax /* Call LoadLibraryExW */
	mov		edi, eax /* Save return value */
	test	ebp, ebp
	jz		SkipDirectoryBS2
	push	esp /* Reset directory to the original one of the process */
	call	esi /* Call SetCurrentDirectoryW */
	add		esp, ebx /* Deallocate buffer from the stack */
SkipDirectoryBS2:
_inject_ExitThreadptr:
	mov		esi, 0xDEADBEEF
	test	edi, edi /* Check whether LoadLibraryEx was successful */
	jz		ThrowError1
_inject_funcnameptr:
	push	0xDEADBEEF /* Push the address of the function name to load */
	push	edi
_inject_GetProcAddressptr:
	mov		eax, 0xDEADBEEF
	call	eax /* Call GetProcAddress */
	test	eax, eax /* Check whether the function was found */
	jz		ThrowError2
_inject_funcparamptr:
	push	0xDEADBEEF /* Push the address of the function parameter */
	call	eax

	/*
	  If we get here, [func_name] has been called,
	  so it's time to close this thread and optionally unload the DLL.
	*/
	
	mov		eax, esi
	pop		edi
	pop		esi
	pop		ebp
	pop		ebx
	push	0 /* Exit code */
	call	eax /* Call ExitThread */

	.balign 16, 0xCC

ThrowError1:
	mov		eax, esi
	pop		edi
	pop		esi
	pop		ebp
	pop		ebx
	push	1 /* Exit code */
	call	eax /* Call ExitThread */
	.balign 16, 0xCC

ThrowError2:
	mov		edx, edi
	pop		edi
	pop		esi
	pop		ebp
	pop		ebx
	push	2 /* Exit code */
	push	edx /* Push the injected DLL's module handle */
_inject_FreeLibraryAndExitThreadptr:
	mov		eax, 0xDEADBEEF
	call	eax /* Call FreeLibraryAndExitThread */
	.balign 16, 0xCC
_inject_end:
