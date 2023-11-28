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

/*

This C++ code is included for readability and does not fully
reflect all of the optimizations implemented in the assembly.

static std::pair<uintptr_t, const char*> TH_FASTCALL str_to_addr_impl(uintptr_t base_addr, const char* str) {
	const uint8_t* str_read = (const uint8_t*)str;

	const uint8_t is_not_relative = 0x01;
	const uint8_t is_overflow = 0x80;

	uint8_t flags = is_not_relative; // BL
	uint8_t number_base = 10; // BH
	uint8_t c; // CL
	switch (c = *str_read) {
		default:
			goto fail;
		case 'r': case 'R':
			flags = 0;
			TH_FALLTHROUGH;
		case '0':
			switch (c = *++str_read) {
				case 'x': case 'X':
					number_base = 16;
					c = *++str_read;
					break;
				case 'b': case 'B':
					number_base = 2;
					c = *++str_read;
					break;
				default:
					return flags == is_not_relative
							? std::make_pair(0, (const char*)str_read)
							: std::make_pair(std::numeric_limits<uintptr_t>::max(), str);
				case '0' ... '9':
					break;
			}
			break;
		case '1' ... '9':
			break;
	}
	uintptr_t ret = 0; // EAX
	for (;; c = *++str_read) {
		uint8_t digit = c; // CL
		switch (digit) {
			case '0' ... '9':
				digit -= '0';
				break;
			case 'a' ... 'f':
				digit -= 'a' - 10;
				break;
			case 'A' ... 'F':
				digit -= 'A' - 10;
				break;
			default:
				goto end_parse;
		}
		if (digit >= number_base) {
			goto end_parse;
		}
		if (__builtin_expect(!(flags & is_overflow), true)) {
			if (__builtin_expect(!__builtin_mul_overflow(ret, number_base, &ret), true)) {
				if (__builtin_expect(!__builtin_add_overflow(ret, digit, &ret), true)) {
					continue;
				}
			}
			flags = is_overflow;
		}
	}
end_parse:
	if (!(flags & is_overflow)) {
		if (!(flags & is_not_relative)) {
			ret += base_addr;
		}
		return std::make_pair(ret, (const char*)str_read);
	}
fail:
	return std::make_pair(std::numeric_limits<uintptr_t>::max(), str);
}

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
	ADD		ESI, 1
	LEA		EAX, [ECX-'0']
	CMP		AL, 10
	JB		is_base_ten
	MOV		BH, 16
	OR		CL, 0x20
	CMP		CL, 'x'
	JE		is_zero_prefix
	CMP		CL, 'b'
	JNE		standalone_zero
	MOV		BH, 2
is_zero_prefix:
	MOVZX	ECX, BYTE PTR [EDX+2]
	ADD		ESI, 1
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
	INT3
standalone_zero:
	CMP		BL, 1 /* Make sure that a single 0 is valid but a single R is not */
	CMOVE	EDX, ESI
	SBB		EAX, EAX /* Returns 0 or -1 */
	POP		EDI
	POP		ESI
	POP		EBP
	POP		EBX
	RET
	INT3
