/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoint entry point. Written for i686-w64-mingw32-as.
  */

	.intel_syntax noprefix

	.global bp_entry0, bp_entry0_jsonptr, bp_entry0_funcptr, bp_entry0_caveptr, bp_entry0_end
	.global bp_entry0s, bp_entry0s_jsonptr, bp_entry0s_funcptr, bp_entry0s_caveptr, bp_entry0s_end, bp_entry0s_retpop
	.global bp_entry1, bp_entry1_jsonptr, bp_entry1_funcptr, bp_entry1_caveptr, bp_entry1_end
	.global bp_entry1s, bp_entry1s_jsonptr, bp_entry1s_funcptr, bp_entry1s_caveptr, bp_entry1s_end, bp_entry1s_retpop
	.global bp_entry2, bp_entry2_jsonptr, bp_entry2_funcptr, bp_entry2_caveptr, bp_entry2_end
	.global bp_entry2s, bp_entry2s_jsonptr, bp_entry2s_funcptr, bp_entry2s_caveptr, bp_entry2s_end, bp_entry2s_retpop
	.global bp_entry3, bp_entry3_jsonptr, bp_entry3_funcptr, bp_entry3_caveptr, bp_entry3_end
	.global bp_entry3s, bp_entry3s_jsonptr, bp_entry3s_funcptr, bp_entry3s_caveptr, bp_entry3s_end, bp_entry3s_retpop

	.macro movRAXQ value
	.byte 0x48
	.byte 0xB8
	.quad \value
	.endm

	.macro movRCXQ value
	.byte 0x48
	.byte 0xB9
	.quad \value
	.endm

	.macro movRDXQ value
	.byte 0x48
	.byte 0xBA
	.quad \value
	.endm

	.macro pusha
	push	rax
	push	rcx
	push	rdx
	push	rbx
	lea		rbx, [rsp+0x28] /* fixed rsp offset */
	push	rbx
	push	rbp
	push	rsi
	push	rdi
	push	r8
	push	r9
	push	r10
	push	r11
	push	r12
	push	r13
	push	r14
	push	r15
	.endm

	.macro popa_except_rax
	pop		r15
	pop		r14
	pop		r13
	pop		r12
	pop		r11
	pop		r10
	pop		r9
	pop		r8
	pop		rdi
	pop		rsi
	pop		rbp
	pop		rcx
	pop		rbx
	pop		rdx
	pop		rcx
	.endm

	.macro popa
	popa_except_rax
	pop		rax
	.endm

	.macro pushf_fast
	lahf
	seto	al
	push	rax
	.endm

	.macro popf_fast
	pop		rax
	cmp		al, 0x81
	sahf
	.endm

bp_entry0:
	pusha
	pushf_fast
	cld
	mov		rbp, rsp
	and		rsp, -16
	sub		rsp, 0x20
	mov		rcx, rbp
bp_entry0_jsonptr:
	movRDXQ	0x0000000000000000
bp_entry0_funcptr:
	movRAXQ	0x0000000000000000
	call	rax
	mov		rsp, rbp
	test	eax, eax
	jz		skip_ret_set0
bp_entry0_caveptr:
	movRCXQ 0x0000000000000000
	mov		qword ptr [rbx-8], rcx
skip_ret_set0:
	popf_fast
	popa
	ret
	.balign 16, 0xCC
bp_entry0_end:

bp_entry0s:
	pusha
	pushf_fast
	cld
	mov		rbp, rsp
	and		rsp, -16
	sub		rsp, 0x20
	mov		rcx, rbp
bp_entry0s_jsonptr:
	movRDXQ	0x0000000000000000
bp_entry0s_funcptr:
	movRAXQ	0x0000000000000000
	call	rax
	mov		rsp, rbp
	test	eax, eax
	pop		rax
	popa_except_rax
	jnz		ret_set0s
	cmp		al, 0x81
	sahf
	pop		rax
bp_entry0s_retpop:
	ret 0
	int3
ret_set0s:
	cmp		al, 0x81
	sahf
bp_entry0s_caveptr:
	movRAXQ 0x0000000000000000
	mov		qword ptr [rsp+8], rax
	pop		rax
	ret
	.balign 16, 0xCC
bp_entry0s_end:

bp_entry1:
	pusha
	pushf
	cld
	mov		rbp, rsp
	and		rsp, -16
	sub		rsp, 0x20
	mov		rcx, rbp
bp_entry1_jsonptr:
	movRDXQ	0x0000000000000000
bp_entry1_funcptr:
	movRAXQ	0x0000000000000000
	call	rax
	mov		rsp, rbp
	test	eax, eax
	jz		skip_ret_set1
bp_entry1_caveptr:
	movRCXQ 0x0000000000000000
	mov		qword ptr [rbx-8], rcx
skip_ret_set1:
	popf
	popa
	ret
	.balign 16, 0xCC
bp_entry1_end:

bp_entry1s:
	pusha
	pushf
	cld
	mov		rbp, rsp
	and		rsp, -16
	sub		rsp, 0x20
	mov		rcx, rbp
bp_entry1s_jsonptr:
	movRDXQ	0x0000000000000000
bp_entry1s_funcptr:
	movRAXQ	0x0000000000000000
	call	rax
	mov		rsp, rbp
	test	eax, eax
	pop		rax
	popa_except_rax
	push	rax
	jnz		ret_set1s
	popf
	pop		rax
bp_entry1s_retpop:
	ret 0
	int3
ret_set1s:
	popf
bp_entry1s_caveptr:
	movRAXQ 0x0000000000000000
	mov		qword ptr [rsp+8], rax
	pop		rax
	ret
	.balign 16, 0xCC
bp_entry1s_end:

bp_entry2:
	pusha
	pushf_fast
	cld
	mov		rbp, rsp
	and		rsp, -16
	sub		rsp, 512 + 0x20
	mov		rcx, rbp
	fxsave64	[rsp-0x20]
bp_entry2_jsonptr:
	movRDXQ	0x0000000000000000
bp_entry2_funcptr:
	movRAXQ	0x0000000000000000
	call	rax
	fxrstor64	[rsp+0x20]
	mov		rsp, rbp
	test	eax, eax
	jz		skip_ret_set2
bp_entry2_caveptr:
	movRCXQ 0x0000000000000000
	mov		qword ptr [ebp+0x24], rcx
skip_ret_set2:
	popf_fast
	popa
	ret
	.balign 16, 0xCC
bp_entry2_end:

bp_entry2s:
	pusha
	pushf_fast
	cld
	mov		rbp, rsp
	and		rsp, -16
	sub		rsp, 512 + 0x20
	mov		rcx, rbp
	fxsave64	[rsp-0x20]
bp_entry2s_jsonptr:
	movRDXQ	0x0000000000000000
bp_entry2s_funcptr:
	movRAXQ	0x0000000000000000
	call	rax
	fxrstor64	[rsp+0x20]
	mov		rsp, rbp
	test	eax, eax
	pop		rax
	popa_except_rax
	jnz		ret_set2s
	cmp		al, 0x81
	sahf
	pop		rax
bp_entry2s_retpop:
	ret 0
	int3
ret_set2s:
	cmp		al, 0x81
	sahf
	pop		rax
bp_entry2s_caveptr:
	movRAXQ 0x0000000000000000
	mov		qword ptr [rsp+8], rax
	pop		rax
	ret
	.balign 16, 0xCC
bp_entry2s_end:

bp_entry3:
	pusha
	pushf
	cld
	mov		rbp, rsp
	and		rsp, -16
	sub		rsp, 512 + 0x20
	mov		rcx, rbp
	fxsave64	[rsp-0x20]
bp_entry3_jsonptr:
	movRDXQ	0x0000000000000000
bp_entry3_funcptr:
	movRAXQ	0x0000000000000000
	call	rax
	fxrstor64	[rsp+0x20]
	mov		rsp, rbp
	test	eax, eax
	jz		skip_ret_set3
bp_entry3_caveptr:
	movRCXQ 0x0000000000000000
	mov		qword ptr [rbx-8], rcx
skip_ret_set3:
	popf
	popa
	ret
	.balign 16, 0xCC
bp_entry3_end:

bp_entry3s:
	pusha
	pushf
	cld
	mov		rbp, rsp
	and		rsp, -16
	sub		rsp, 512 + 0x20
	mov		rcx, rbp
	fxsave64	[rsp-0x20]
bp_entry3s_jsonptr:
	movRDXQ	0x0000000000000000
bp_entry3s_funcptr:
	movRAXQ	0x0000000000000000
	call	rax
	fxrstor64	[rsp+0x20]
	mov		rsp, rbp
	test	eax, eax
	pop		rax
	popa_except_rax
	push	rax
	jnz		ret_set3s
	popf
	pop		rax
bp_entry3s_retpop:
	ret 0
	int3
ret_set3s:
	popf
bp_entry3s_caveptr:
	movRAXQ 0x0000000000000000
	mov		qword ptr [rsp+8], rax
	pop		rax
	ret
	.balign 16, 0xCC
bp_entry3s_end:
