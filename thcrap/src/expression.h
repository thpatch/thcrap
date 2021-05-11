/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Expression parsing
  */

#pragma once
#include <uchar.h>

// Register structure in PUSHAD+PUSHFD order at the beginning of a function
typedef struct {
	union {
		uint32_t eflags;
		uint16_t flags;
	};
	union {
		uint32_t edi;
		uint16_t di;
	};
	union {
		uint32_t esi;
		uint16_t si;
	};
	union {
		uint32_t ebp;
		uint16_t bp;
	};
	union {
		uint32_t esp;
		uint16_t sp;
	};
	union {
		uint32_t ebx;
		uint16_t bx;
		struct {
			uint8_t bl;
			uint8_t bh;
		};
	};
	union {
		uint32_t edx;
		uint16_t dx;
		struct {
			uint8_t dl;
			uint8_t dh;
		};
	};
	union {
		uint32_t ecx;
		uint16_t cx;
		struct {
			uint8_t cl;
			uint8_t ch;
		};
	};
	union {
		uint32_t eax;
		uint16_t ax;
		struct {
			uint8_t al;
			uint8_t ah;
		};
	};
	uint32_t retaddr;
} x86_reg_t;

// Enum of possible types for the description of
// a value specified by the user defined patch options
enum {
	PVT_NONE = 0,
	PVT_BYTE = 1,
	PVT_SBYTE = 2,
	PVT_WORD = 3,
	PVT_SWORD = 4,
	PVT_DWORD = 5,
	PVT_SDWORD = 6,
	PVT_QWORD = 7,
	PVT_SQWORD = 8,
	PVT_FLOAT = 9,
	PVT_DOUBLE = 10,
	PVT_LONGDOUBLE = 11,
	PVT_STRING,
	PVT_STRING8 = PVT_STRING,
	PVT_STRING16,
	PVT_STRING32,
	PVT_CODE,
	PVT_ADDRRET
};
typedef uint8_t patch_value_type_t;

// Description of a value specified by the options
typedef union {

	// Note: This is implemented as a struct within the main union
	// rather than as a field in an outer struct since the compiler
	// insisted on aligning things weirdly and taking up ~24 bytes of
	// data for a 16 byte struct, thus preventing XMM move optimizations.
	struct {
		unsigned char raw_bytes[12];
		patch_value_type_t type;
		uint8_t merge_op; // Op values not exposed outside expression.cpp
		// Room for 2 more bytes of flags or other data
	};

	uint8_t b;
	int8_t sb;
	uint16_t w;
	int16_t sw;
	uint32_t i;
	int32_t si;
	uint64_t q;
	int64_t sq;
	uintptr_t p;
	float f;
	double d;
	LongDouble80 ld;
	struct {
		const char* ptr;
		size_t len;
	} str;
	struct {
		const char16_t* ptr;
		size_t len;
	} str16;
	struct {
		const char32_t* ptr;
		size_t len;
	} str32;
	struct {
		const char* ptr;
		size_t len;
		size_t count;
	} code;

	// Note: This isn't *really* supposed to be a part
	// of this union, but consume_value was struggling
	// to optimize away a bunch of variables.
	str_address_ret_t addr_ret;

} patch_val_t;

// Parses [expr], a string containing a [relative] or <absolute> patch value and writes it [out].
// Returns a pointer to the character following the parsed patch value or NULL on error.
// [regs] is either the current register structure if called from a breakpoint or null.
// [rel_source] is the address used when computing a relative value.
const char* get_patch_value(const char* expr, patch_val_t* out, x86_reg_t* regs, size_t rel_source);

void DisableCodecaveNotFoundWarning(bool state);

// Returns a pointer to the register [regname] in [regs]. [endptr] behaves
// like the endptr parameter of strtol(), and can be a nullptr if not needed.
size_t* reg(x86_reg_t *regs, const char *regname, const char **endptr);

const char* parse_brackets(const char* str, char c);

// Parses [expr], a string containing an expression terminated by [end].
// Returns a pointer to the character following the parsed expression or NULL on error.
// [regs] is either the current register structure if called from a breakpoint or null.
// [rel_source] is the address used when computing a relative value.
const char* TH_FASTCALL eval_expr(const char* expr, char end, size_t* out, x86_reg_t* regs, size_t rel_source);

void patch_val_set_op(const char* op_str, patch_val_t* Val);

patch_val_t patch_val_op_str(const char* op_str, patch_val_t Val1, patch_val_t Val2);
