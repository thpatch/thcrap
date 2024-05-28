/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoint entry point. Written for i686-w64-mingw32-as.
  */

	.intel_syntax noprefix

	.global _bp_entry0, _bp_entry0_jsonptr, _bp_entry0_funcptr, _bp_entry0_caveptr, _bp_entry0_end
	.global _bp_entry0s, _bp_entry0s_jsonptr, _bp_entry0s_funcptr, _bp_entry0s_caveptr, _bp_entry0s_end, _bp_entry0s_retpop
	.global _bp_entry1, _bp_entry1_jsonptr, _bp_entry1_funcptr, _bp_entry1_caveptr, _bp_entry1_end
	.global _bp_entry1s, _bp_entry1s_jsonptr, _bp_entry1s_funcptr, _bp_entry1s_caveptr, _bp_entry1s_end, _bp_entry1s_retpop
	.global _bp_entry2, _bp_entry2_jsonptr, _bp_entry2_funcptr, _bp_entry2_caveptr, _bp_entry2_end
	.global _bp_entry2s, _bp_entry2s_jsonptr, _bp_entry2s_funcptr, _bp_entry2s_caveptr, _bp_entry2s_end, _bp_entry2s_retpop
	.global _bp_entry3, _bp_entry3_jsonptr, _bp_entry3_funcptr, _bp_entry3_caveptr, _bp_entry3_end
	.global _bp_entry3s, _bp_entry3s_jsonptr, _bp_entry3s_funcptr, _bp_entry3s_caveptr, _bp_entry3s_end, _bp_entry3s_retpop

	.macro pushDW value
	.byte 0x68
	.int \value
	.endm

	.macro pushf_fast
	lahf
	seto	al
	push	eax
	.endm

	.macro popf_fast
	pop		eax
	cmp		al, 0x81
	sahf
	.endm

_bp_entry0:
	pusha
	pushf_fast
	cld
	mov		ebp, esp
_bp_entry0_jsonptr:
	pushDW	0x00000000
	push	ebp
_bp_entry0_funcptr:
	call	_bp_entry0
	mov		esp, ebp
	test	eax, eax
	jz		skip_ret_set0
_bp_entry0_caveptr:
	mov		dword ptr [ebp+0x24], 0
skip_ret_set0:
	popf_fast
	popa
	ret
	.balign 16, 0xCC
_bp_entry0_end:

_bp_entry0s:
	pusha
	pushf_fast
	cld
	mov		ebp, esp
_bp_entry0s_jsonptr:
	pushDW	0x00000000
	push	ebp
_bp_entry0s_funcptr:
	call	_bp_entry0s
	mov		esp, ebp
	test	eax, eax
	pop		eax
	jnz		ret_set0s
	cmp		al, 0x81
	sahf
	popa
_bp_entry0s_retpop:
	ret 0
	int3
ret_set0s:
	cmp		al, 0x81
	sahf
	popa
_bp_entry0s_caveptr:
	mov		dword ptr [esp], 0
	ret
	.balign 16, 0xCC
_bp_entry0s_end:

_bp_entry1:
	pusha
	pushf
	cld
	mov		ebp, esp
_bp_entry1_jsonptr:
	pushDW	0x00000000
	push	ebp
_bp_entry1_funcptr:
	call	_bp_entry1
	mov		esp, ebp
	test	eax, eax
	jz		skip_ret_set1
_bp_entry1_caveptr:
	mov		dword ptr [ebp+0x24], 0
skip_ret_set1:
	popf
	popa
	ret
	.balign 16, 0xCC
_bp_entry1_end:

_bp_entry1s:
	pusha
	pushf
	cld
	mov		ebp, esp
_bp_entry1s_jsonptr:
	pushDW	0x00000000
	push	ebp
_bp_entry1s_funcptr:
	call	_bp_entry1s
	mov		esp, ebp
	test	eax, eax
	jnz		ret_set1s
	popf
	popa
_bp_entry1s_retpop:
	ret 0
	int3
ret_set1s:
	popf
	popa
_bp_entry1s_caveptr:
	mov		dword ptr [esp], 0
	ret
	.balign 16, 0xCC
_bp_entry1s_end:

_bp_entry2:
	pusha
	pushf_fast
	cld
	mov		ebp, esp
	and		esp, -16
	sub		esp, 512
	fxsave	[esp]
_bp_entry2_jsonptr:
	pushDW	0x00000000
	push	ebp
_bp_entry2_funcptr:
	call	_bp_entry2
	fxrstor	[esp+8]
	mov		esp, ebp
	test	eax, eax
	jz		skip_ret_set2
_bp_entry2_caveptr:
	mov		dword ptr [ebp+0x24], 0
skip_ret_set2:
	popf_fast
	popa
	ret
	.balign 16, 0xCC
_bp_entry2_end:

_bp_entry2s:
	pusha
	pushf_fast
	cld
	mov		ebp, esp
	and		esp, -16
	sub		esp, 512
	fxsave	[esp]
_bp_entry2s_jsonptr:
	pushDW	0x00000000
	push	ebp
_bp_entry2s_funcptr:
	call	_bp_entry2s
	fxrstor	[esp+8]
	mov		esp, ebp
	test	eax, eax
	pop		eax
	jnz		ret_set2s
	cmp		al, 0x81
	sahf
	popa
_bp_entry2s_retpop:
	ret 0
	int3
ret_set2s:
	cmp		al, 0x81
	sahf
	popa
_bp_entry2s_caveptr:
	mov		dword ptr [esp], 0
	ret
	.balign 16, 0xCC
_bp_entry2s_end:

_bp_entry3:
	pusha
	pushf
	cld
	mov		ebp, esp
	and		esp, -16
	sub		esp, 512
	fxsave	[esp]
_bp_entry3_jsonptr:
	pushDW	0x00000000
	push	ebp
_bp_entry3_funcptr:
	call	_bp_entry3
	fxrstor	[esp+8]
	mov		esp, ebp
	test	eax, eax
	jz		skip_ret_set3
_bp_entry3_caveptr:
	mov		dword ptr [ebp+0x24], 0
skip_ret_set3:
	popf
	popa
	ret
	.balign 16, 0xCC
_bp_entry3_end:

_bp_entry3s:
	pusha
	pushf
	cld
	mov		ebp, esp
	and		esp, -16
	sub		esp, 512
	fxsave	[esp]
_bp_entry3s_jsonptr:
	pushDW	0x00000000
	push	ebp
_bp_entry3s_funcptr:
	call	_bp_entry3s
	fxrstor	[esp+8]
	mov		esp, ebp
	test	eax, eax
	jnz		ret_set3s
	popf
	popa
_bp_entry3s_retpop:
	ret 0
	int3
ret_set3s:
	popf
	popa
_bp_entry3s_caveptr:
	mov		dword ptr [esp], 0
	ret
	.balign 16, 0xCC
_bp_entry3s_end:
