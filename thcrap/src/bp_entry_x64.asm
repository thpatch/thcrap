/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoint entry point. Written for i686-w64-mingw32-as.
  */

	.intel_syntax noprefix
	.global	bp_entry, bp_entry_indexptr, bp_entry_localptr, bp_entry_callptr, bp_entry_end

bp_entry:
	push	rax
	push	rcx
	lea		rcx, [rsp+0x10]
	push	rdx
	push	rbx
	push	rcx
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
	pushf
	cld
	mov		r8, rsp
	sub		rsp, 0x20
bp_entry_indexptr:
	mov		edx, 0x00000000
bp_entry_localptr:
	movabs	rcx, 0x0000000000000000
bp_entry_callptr:
	movabs	rax, 0x0000000000000000
	call	rax
	lea		rsp, [rsp+rax+0x20]
	popf
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
	pop		rax
	ret
	.balign 16, 0xCC
bp_entry_end:
