/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Stack probe helper functions
  */

	.intel_syntax noprefix

	.global _initialize_wow_exception_hooks

ntdll_name:
	.short 'n', 't', 'd', 'l', 'l', '.', 'd', 'l', 'l', 0
wow64_name:
	.short 'w', 'o', 'w', '6', '4', '.', 'd', 'l', 'l', 0
prepare_exception_name:
	.byte 'W', 'o', 'w', '6', '4', 'P', 'r', 'e', 'p', 'a', 'r', 'e', 'F', 'o', 'r', 'E', 'x', 'c', 'e', 'p', 't', 'i', 'o', 'n', 0
protect_memory_name:
	.byte 'N', 't', 'P', 'r', 'o', 't', 'e', 'c', 't', 'V', 'i', 'r', 't', 'u', 'a', 'l', 'M', 'e', 'm', 'o', 'r', 'y', 0
	.balign 16, 0xCC

	.code64

_initialize_wow_exception_hooks:
	MOV EBX, ESP
	SUB ESP, 0x40
	AND ESP, -16

	MOV RAX, QWORD PTR GS:[0x60]
	MOV R9, QWORD PTR [RAX + 0x18]
	MOV RDX, QWORD PTR [R9 + 0x10]
	ADD R9, 0x10
	XOR EAX, EAX
	XOR R8D, R8D
	XOR R11D, R11D
	JMP start_module_loop

	.balign 16, 0xCC

	// Find the 64 bit ntdll.dll and wow64.dll

check_ntdll:
	MOV RDI, QWORD PTR [RDX + 0x60]
	MOV ECX, 18
	MOV ESI, OFFSET ntdll_name
	REPE CMPSB
	JNE not_ntdll
	MOV R8, QWORD PTR [RDX + 0x30]
	TEST RAX, RAX
	JNZ both_modules_found

.align 16

next_module:
	MOV RDX, QWORD PTR [RDX]
	CMP RDX, R9
	JE initialize_hook_fail
start_module_loop:
	CMP WORD PTR [RDX + 0x58], 18
	JNE next_module
	TEST R8, R8
	JZ check_ntdll
not_ntdll:
	TEST RAX, RAX
	JNZ next_module
	MOV RDI, QWORD PTR [RDX + 0x60]
	MOV ECX, 18
	MOV ESI, OFFSET wow64_name
	REPE CMPSB
	JNE next_module
	MOV RAX, QWORD PTR [RDX + 0x30]
	TEST R8, R8
	JZ next_module
both_modules_found:
	MOV ECX, DWORD PTR [RAX + 0x3C]
	MOV R9D, DWORD PTR [RAX*1 + RCX + 0x88]
	MOV EDX, DWORD PTR [RAX*1 + R9 + 0x18]
	MOV R10D, DWORD PTR [RAX*1 + R9 + 0x20]
	ADD R10, RAX

.align 16

	// Find NtProtectVirtualMemory and Wow64PrepareForException

next_wow64_export:
	SUB EDX, 1
	JC initialize_hook_fail
	MOV EDI, [RDX*4 + R10]
	ADD RDI, RAX
	MOV ECX, 25
	MOV ESI, OFFSET prepare_exception_name
	REPE CMPSB
	JNE next_wow64_export
	MOV ECX, DWORD PTR [RAX*1 + R9 + 0x1C]
	MOV ESI, DWORD PTR [RAX*1 + R9 + 0x24]
	ADD RSI, RAX
	MOVZX EDX, WORD PTR [RDX*2 + RSI]
	ADD RCX, RAX
	MOV EDI, DWORD PTR [RDX*4 + RCX]
	ADD RAX, RDI
	MOV EDX, DWORD PTR [R8 + 0x3C]
	MOV R9D, DWORD PTR [R8*1 + RDX + 0x88]
	ADD RDX, R8
	MOV R11D, DWORD PTR [R8*1 + R9 + 0x18]
	MOV R10D, DWORD PTR [R8*1 + R9 + 0x20]
	ADD R10, R8

.align 16

next_ntdll_export:
	SUB R11D, 1
	JC initialize_hook_fail
	MOV EDI, DWORD PTR [R11*4 + R10]
	ADD RDI, R8
	MOV ECX, 23
	MOV ESI, OFFSET protect_memory_name
	REPE CMPSB
	JNE next_ntdll_export
	MOV ECX, DWORD PTR [R8*1 + R9 + 0x1C]
	MOV EDI, DWORD PTR [R8*1 + R9 + 0x24]
	ADD RDI, R8
	MOVZX ESI, WORD PTR [R11*2 + RDI]
	ADD RCX, R8
	MOV R9D, DWORD PTR [RSI*4 + RCX]
	ADD R9, R8
	MOVZX ESI, WORD PTR [RDX + 0x14]
	ADD RSI, RDX
	MOVZX EDX, WORD PTR [EDX + 0x6]
	XOR R11D, R11D
	JMP start_section_loop

	.balign 16, 0xCC

	// Scan the entire ntdll.dll to find where the pointer to Wow64PrepareForException was written

next_section:
	ADD RSI, 0x28
	DEC EDX
	JZ initialize_hook_fail
start_section_loop:
	MOVZX ECX, BYTE PTR [RSI + 0x3F]
	AND CL, 0x60
	CMP CL, 0x40
	JNE next_section
	MOV ECX, DWORD PTR [RSI + 0x20]
	MOV EDI, DWORD PTR [RSI + 0x24]
	ADD RDI, R8
	SHR ECX, 3
	REPNE SCASQ
	JNE next_section

	// Found the correct address, remap it as writable and attempt to hook

	// MOV QWORD PTR [original_prepare_addr], RAX
	.byte 0x48, 0x89, 0x04, 0x25
	.int _original_prepare_addr
	MOV RSI, R9
	ADD RDI, -8
	MOV QWORD PTR [RSP + 0x38], RDI
	MOV QWORD PTR [RSP + 0x30], 8
	LEA EAX, [RSP + 0x2C]
	MOV QWORD PTR [RSP + 0x20], RAX
	LEA EDX, [RSP + 0x38]
	LEA R8D, [RSP + 0x30]
	MOV RCX, -1
	MOV R9D, 4
	CALL RSI
	TEST EAX, EAX
	JS initialize_hook_fail2
	MOV EAX, OFFSET prepare_for_exception_hook
	// XCHG isn't a mistake here, it's supposed to be atomic
	XCHG RAX, QWORD PTR [RDI]
	MOV R9D, DWORD PTR [RSP + 0x2C]
	LEA EAX, [RSP + 0x2C]
	MOV QWORD PTR [RSP + 0x20], RAX
	LEA EDX, [RSP + 0x38]
	LEA R8D, [RSP + 0x30]
	MOV RCX, -1
	CALL RSI

	MOV R11D, 1
initialize_hook_fail:
	MOV EAX, R11D
initialize_hook_fail2:
	MOV ESP, EBX
	RETF

	.balign 16, 0xCC

	// This hook reads the exception pointers from the x64 CONTEXT
	// and copies them to 32 bit TLS. There aren't any other good
	// places to store the data across the WoW boundry while still
	// keeping threads isolated from each other.
	// Jumps into the original WoW64 exception handler at the end.

prepare_for_exception_hook:
	// MOV EAX, DWORD PTR [exception_to_tls]
	.byte 0x8B, 0x04, 0x25
	.int _exception_to_tls
	MOV R8D, DWORD PTR [RDX + 0x4C0]
	CMP EAX, 64
	JNB ext_tls_except_to
	MOV DWORD PTR FS:[RAX*4 + 0xE10], R8D
	JMP tls_except_from

ext_tls_except_to:
	MOV R9D, DWORD PTR FS:[0xF94]
	MOV R8D, DWORD PTR [RAX*4 + R9 - 0x100]
tls_except_from:
	// MOV EAX, DWORD PTR [exception_from_tls]
	.byte 0x8B, 0x04, 0x25
	.int _exception_from_tls
	MOV R8D, DWORD PTR [RDX + 0x4C8]
	CMP EAX, 64
	JNB ext_tls_except_from
	MOV DWORD PTR FS:[RAX*4 + 0xE10], R8D
	JMP call_original_prepare

ext_tls_except_from:
	MOV R9D, DWORD PTR FS:[0xF94]
	MOV R8D, DWORD PTR [RAX*4 + R9 - 0x100]
call_original_prepare:
	// JMP QWORD PTR [original_prepare_addr]
	.byte 0xFF, 0x24, 0x25
	.int _original_prepare_addr

	.balign 16, 0xCC
