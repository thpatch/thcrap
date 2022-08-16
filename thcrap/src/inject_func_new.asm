	.intel_syntax noprefix
	.global _inject, _inject_ExitThreadptr, _inject_dllpathptr, _inject_LoadLibraryExWptr, _inject_funcnameptr, _inject_GetProcAddressptr, _inject_funcparamptr, _inject_FreeLibraryAndExitThreadptr, _inject_end

_inject:
	int3
	push esi
	push edi
_inject_ExitThreadptr:
	mov edi, 0xDEADBEEF
	push 8
	push 0
_inject_dllpathptr:
	push 0xDEADBEEF
_inject_LoadLibraryExWptr:
	mov eax, 0xDEADBEEF
	call eax
	test eax, eax
	jz ThrowError1
	mov esi, eax
_inject_funcnameptr:
	push 0xDEADBEEF
	push eax
_inject_GetProcAddressptr:
	mov eax, 0xDEADBEEF
	call eax
	test eax, eax
	jz ThrowError2
_inject_funcparamptr:
	push 0xDEADBEEF
	call eax
	push 0
	call edi
ThrowError1:
	push 1
	call edi
ThrowError2:
	push 2
	push esi
_inject_FreeLibraryAndExitThreadptr:
	mov eax, 0xDEADBEEF
	call eax
.balign 16, 0xCC
_inject_end:
