/*
	//------------------------------------------//
	// Injected dll loading.                    //
	//------------------------------------------//
	
	This is how following assembly code would look like in C/C++

	// In case the injected DLL depends on other DLLs,
	// we need to change the current directory to the one given as parameter
	wchar_t* dll_dir_ptr = dll_dir;
	wchar_t* cur_dir;
	if(dll_dir_ptr) {
		size_t cur_dir_len = GetCurrentDirectoryW(0, NULL);
		cur_dir = (wchar_t*)_alloca(sizeof(wchar_t[cur_dir_len]));
		GetCurrentDirectoryW(cur_dir_len, cur_dir);
		SetCurrentDirectoryW(dll_dir_ptr);
	}

	// Load the injected DLL into this process
	HMODULE h = LoadLibraryEx(dll_fn, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
	if(!h) {
		ExitThread(1);
	}

	if(dll_dir_ptr) {
		SetCurrentDirectoryW(cur_dir);
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
	.global inject, inject_dlldirptr, inject_loadlibraryflagsptr, inject_dllnameptr, inject_funcnameptr, inject_funcparamptr, inject_end

	/*
	  Note: All of these functions use __stdcall, thus
	  cleaning their own parameters off the stack.
	*/
	.global inject_GetCurrentDirectoryWptr, inject_SetCurrentDirectoryWptr, inject_LoadLibraryExWptr, inject_ExitThreadptr, inject_GetProcAddressptr, inject_FreeLibraryAndExitThreadptr

inject:
	push	rbp
	push	rsi
	push	rdi
	push	rbx
	sub		rsp, 0x28
inject_dlldirptr:
	lea		rbp, [inject_dlldirptr]
	test	rbp, rbp
	jz		SkipDirectoryBS1

	/* Get length for current directory */

inject_GetCurrentDirectoryWptr:
	movabs	rsi, 0xDEADBEEFDEADBEEF
	xor		ecx, ecx
	xor		edx, edx
	call	rsi /* Call GetCurrentDirectoryW */
	lea		rbx, [rax + rax + 0x2F] /* Calculate byte size of directory buffer. */
	and		rbx, -16 /* Also do some boundary alignment. */
	sub		rsp, rbx /* Allocate a suitable buffer on stack. */
	lea		rdx, [rsp + 0x20]
	mov		rcx, rax
	call	rsi /* Call GetCurrentDirectoryW */
	mov		rcx, rbp /* Push the address of our directory */
inject_SetCurrentDirectoryWptr:
	movabs	rsi, 0xDEADBEEFDEADBEEF
	call	rsi /* Call SetCurrentDirectoryW */
SkipDirectoryBS1:
inject_dllnameptr:
	lea		rcx, [inject_dllnameptr] /* Push the address of the DLL name to use in LoadLibraryEx */
	xor		edx, edx
inject_loadlibraryflagsptr:
	mov		r8d, 0x00000000
inject_LoadLibraryExWptr:
	movabs	rax, 0xDEADBEEFDEADBEEF
	call	rax /* Call LoadLibraryExW */
	mov		rdi, rax /* Save return value */
	test	rbp, rbp
	jz		SkipDirectoryBS2
	lea		rcx, [rsp + 0x20] /* Reset directory to the original one of the process */
	call	rsi /* Call SetCurrentDirectoryW */
	add		rsp, rbx /* Deallocate buffer from the stack */
inject_ExitThreadptr:
	movabs	rsi, 0xDEADBEEFDEADBEEF
SkipDirectoryBS2:
	test	rdi, rdi /* Check whether LoadLibraryEx was successful */
	jz		ThrowError1
inject_funcnameptr:
	lea		rdx, [inject_funcnameptr] /* Push the address of the function name to load */
	mov		rcx, rdi
inject_GetProcAddressptr:
	movabs	rax, 0xDEADBEEFDEADBEEF
	call	rax /* Call GetProcAddress */
	test	rax, rax /* Check whether the function was found */
	jz		ThrowError2
inject_funcparamptr:
	lea		rcx, [inject_funcparamptr] /* Push the address of the function parameter */
	call	rax

	/*
	  If we get here, [func_name] has been called,
	  so it's time to close this thread and optionally unload the DLL.
	*/

	xor		ecx, ecx /* Exit code */
	mov		rax, rsi
	add		rsp, 0x28
	pop		rbx
	pop		rdi
	pop		rsi
	pop		rbp
	call	rax /* Call ExitThread */

	.balign 16, 0xCC

ThrowError1:
	mov		ecx, 1 /* Exit code */
	mov		rax, rsi
	add		rsp, 0x28
	pop		rbx
	pop		rdi
	pop		rsi
	pop		rbp
	call	rax /* Call ExitThread */
	.balign 16, 0xCC

ThrowError2:
	mov		rdx, rdi /* Push the injected DLL's module handle */
	mov		ecx, 2 /* Exit code */
	add		rsp, 0x28
	pop		rbx
	pop		rdi
	pop		rsi
	pop		rbp
inject_FreeLibraryAndExitThreadptr:
	movabs	rax, 0xDEADBEEFDEADBEEF
	call	rax /* Call FreeLibraryAndExitThread */
	.balign 16, 0xCC
inject_end:
