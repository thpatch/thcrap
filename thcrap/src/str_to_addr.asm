/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Fast string to address function
  *
  * Returns the integer result in EAX and the end pointer in EDX.
  * If a parsing error or overflow occurs,
  * returns -1 in EAX and the original string pointer in EDX.
  */

	.intel_syntax
	.global @str_to_addr_impl@8

/*
; Fastcall
; ECX: Base
; EDX: str
*/
.align 16, 0xCC
@str_to_addr_impl@8:
	PUSH EBX
	PUSH EBP
	PUSH ESI
	PUSH EDI

/*
	; EAX: Scratch
	; ECX: Current character
	; EDX: str
	; BL: flags (Sign bit: has_overflowed, First bit: is_not_relative)
	; BH: number_base
	; EBP: Stored base
	; ESI: str_read
*/

	MOV		EBP, ECX
	MOV		EBX, 0x0A01
	MOV		ESI, EDX
	MOVZX	ECX, BYTE PTR [EDX]
	LEA		EAX, [ECX-'1']
	CMP		AL, 9
	JB		is_base_ten
	CMP		CL, '0'
	JE		is_leading_zero
	OR		CL, 0x20
	CMP		CL, 'r'
	JNE		failA
	XOR		BL, BL
is_leading_zero:
	MOVZX	ECX, BYTE PTR [EDX+1]
	INC		ESI
	LEA		EAX, [ECX-'0']
	CMP		AL, 10
	JB		is_base_ten
	MOV		BH, 16
	OR		CL, 0x20
	CMP		CL, 'x'
	JE		is_zero_prefix
	MOV		BH, 2
	CMP		CL, 'b'
	JE		is_zero_prefix
	TEST	BL, BL /* Make sure that a single 0 is valid but a single R is not */
	JZ		failA
is_zero_prefix:
	MOVZX	ECX, BYTE PTR [EDX+2]
	INC		ESI
is_base_ten:
/*
	; EAX: ret
	; EDX: number_base
	; EDI: str
*/
	MOV		EDI, EDX
	XOR		EAX, EAX
digit_loop: /* This loop is aligned despite having no padding */
	ADD		CL, -'0'
	CMP		CL, 10
	JB		is_decimal_digit
	ADD		CL, -('A'-'0')
	CMP		CL, 6
	JB		is_upper_hex_digit
	ADD		CL, -('a'-'A')
	CMP		CL, 6
	JAE		end_parse
is_upper_hex_digit:
	ADD		CL, 10
is_decimal_digit:
	CMP		CL, BH
	JAE		end_parse
	TEST	BL, BL
	JS		dont_set_overflow
	MOVZX	EDX, BH
	MUL		EDX
	JO		set_overflow
	ADD		EAX, ECX
	JNC		dont_set_overflow
set_overflow:
	MOV		BL, 0x80 /* Clobbers the relative bit, but that won't be checked once an overflow happens */
dont_set_overflow:
	MOVZX	ECX, BYTE PTR [ESI+1]
	INC		ESI
	JMP		digit_loop
	INT3
end_parse:
	TEST	BL, BL
	JS		failB
/*
	; EAX: number return
	; ECX: scratch
	; EDX: end_str return
*/
	LEA		ECX, [EAX+EBP]
	CMOVZ	EAX, ECX
	MOV		EDX, ESI
	POP		EDI
	POP		ESI
	POP		EBP
	POP		EBX
	RET
	INT3
/*
	; EAX: number return
	; EDX: str return
*/
failB:
	MOV		EDX, EDI
failA:
	MOV		EAX, -1
	POP		EDI
	POP		ESI
	POP		EBP
	POP		EBX
	RET
	INT3
