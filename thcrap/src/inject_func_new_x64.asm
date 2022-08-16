	.intel_syntax noprefix
	.global inject, inject_ExitThreadptr, inject_dllpathptr, inject_LoadLibraryExWptr, inject_funcnameptr, inject_GetProcAddressptr, inject_funcparamptr, inject_FreeLibraryAndExitThreadptr, inject_end

inject:
	push rsi
	push rdi
	sub rsp, 0x28
inject_ExitThreadptr:
	movabs rdi, 0xDEADBEEFDEADBEEF
inject_dllpathptr:
	movabs rcx, 0xDEADBEEFDEADBEEF
	xor edx, edx
	mov r8d, 8
inject_LoadLibraryExWptr:
	movabs rax, 0xDEADBEEFDEADBEEF
	call rax
	test rax, rax
	jz ThrowError1
	mov rsi, rax
inject_funcnameptr:
	movabs rdx, 0xDEADBEEFDEADBEEF
	mov rcx, rax
inject_GetProcAddressptr:
	movabs rax, 0xDEADBEEFDEADBEEF
	call rax
	test rax, rax
	jz ThrowError2
inject_funcparamptr:
	movabs rcx, 0xDEADBEEFDEADBEEF
	call rax
	xor ecx, ecx
	call rdi
ThrowError1:
	mov ecx, 1
	call rdi
ThrowError2:
	mov rcx, rsi
	mov edx, 2
inject_FreeLibraryAndExitThreadptr:
	movabs rax, 0xDEADBEEFDEADBEEF
	call rax
.balign 16, 0xCC
inject_end:
