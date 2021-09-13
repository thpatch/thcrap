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
#ifdef TH_X64
		uint64_t rflags;
#endif
		uint32_t eflags;
		uint16_t flags;
		uint8_t low_flags;
	};
#ifdef TH_X64
	union {
		uint64_t r15;
		uint32_t r15d;
		uint16_t r15w;
		uint8_t r15b;
	};
	union {
		uint64_t r14;
		uint32_t r14d;
		uint16_t r14w;
		uint8_t r14b;
	};
	union {
		uint64_t r13;
		uint32_t r13d;
		uint16_t r13w;
		uint8_t r13b;
	};
	union {
		uint64_t r12;
		uint32_t r12d;
		uint16_t r12w;
		uint8_t r12b;
	};
	union {
		uint64_t r11;
		uint32_t r11d;
		uint16_t r11w;
		uint8_t r11b;
	};
	union {
		uint64_t r10;
		uint32_t r10d;
		uint16_t r10w;
		uint8_t r10b;
	};
	union {
		uint64_t r9;
		uint32_t r9d;
		uint16_t r9w;
		uint8_t r9b;
	};
	union {
		uint64_t r8;
		uint32_t r8d;
		uint16_t r8w;
		uint8_t r8b;
	};
#endif
	union {
#ifdef TH_X64
		uint64_t rdi;
		uint8_t dil;
#endif
		uint32_t edi;
		uint16_t di;
	};
	union {
#ifdef TH_X64
		uint64_t rsi;
		uint8_t sil;
#endif
		uint32_t esi;
		uint16_t si;
	};
	union {
#ifdef TH_X64
		uint64_t rbp;
		uint8_t bpl;
#endif
		uint32_t ebp;
		uint16_t bp;
	};
	union {
#ifdef TH_X64
		uint64_t rsp;
		uint8_t spl;
#endif
		uint32_t esp;
		uint16_t sp;
	};
	union {
#ifdef TH_X64
		uint64_t rbx;
#endif
		uint32_t ebx;
		uint16_t bx;
		struct {
			uint8_t bl;
			uint8_t bh;
		};
	};
	union {
#ifdef TH_X64
		uint64_t rdx;
#endif
		uint32_t edx;
		uint16_t dx;
		struct {
			uint8_t dl;
			uint8_t dh;
		};
	};
	union {
#ifdef TH_X64
		uint64_t rcx;
#endif
		uint32_t ecx;
		uint16_t cx;
		struct {
			uint8_t cl;
			uint8_t ch;
		};
	};
	union {
#ifdef TH_X64
		uint64_t rax;
#endif
		uint32_t eax;
		uint16_t ax;
		struct {
			uint8_t al;
			uint8_t ah;
		};
	};
	uintptr_t retaddr;
} x86_reg_t;

// Enum of possible types for the description of
// a value specified by the user defined patch options
enum {
	PVT_NONE = 0, PVT_UNKNOWN = PVT_NONE,
	PVT_BYTE = 1, PVT_BOOL = PVT_BYTE,
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
#ifdef TH_X64
	PVT_DEFAULT = PVT_QWORD,
	PVT_POINTER = PVT_QWORD,
#else
	PVT_DEFAULT = PVT_DWORD,
	PVT_POINTER = PVT_DWORD,
#endif
	PVT_STRING = 12,
	PVT_STRING8 = PVT_STRING,
	PVT_STRING16 = 13,
	PVT_STRING32 = 14,
	PVT_CODE = 15,
	PVT_ADDRRET = 16
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
	size_t z; // Default
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
const char* get_patch_value(const char* expr, patch_val_t* out, x86_reg_t* regs, uintptr_t rel_source);

bool CPU_Supports_SHA(void);
bool CPU_FDP_ErrorOnly(void);
bool CPU_FCS_FDS_Deprecated(void);

void DisableCodecaveNotFoundWarning(bool state);

// Returns a pointer to the register [regname] in [regs]. [endptr] behaves
// like the endptr parameter of strtol(), and can be a nullptr if not needed.
uint32_t* reg(x86_reg_t *regs, const char *regname, const char **endptr);

// Parses [expr], a string containing an expression terminated by [end].
// Returns a pointer to the character following the parsed expression or NULL on error.
// [regs] is either the current register structure if called from a breakpoint or null.
// [rel_source] is the address used when computing a relative value.
const char* TH_FASTCALL eval_expr(const char* expr, char end, size_t* out, x86_reg_t* regs, uintptr_t rel_source);

void patch_val_set_op(const char* op_str, patch_val_t* Val);

patch_val_t patch_val_op_str(const char* op_str, patch_val_t Val1, patch_val_t Val2);

// Converts a JSON value to a code string
// String returned by this function is guaranteed to persist
const char* code_to_str(json_t* code, const char* name);
