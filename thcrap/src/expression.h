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
		uintptr_t di_ptr;
#ifdef TH_X64
		uint64_t rdi;
		uint8_t dil;
#endif
		uint32_t edi;
		uint16_t di;
	};
	union {
		uintptr_t si_ptr;
#ifdef TH_X64
		uint64_t rsi;
		uint8_t sil;
#endif
		uint32_t esi;
		uint16_t si;
	};
	union {
		uintptr_t bp_ptr;
#ifdef TH_X64
		uint64_t rbp;
		uint8_t bpl;
#endif
		uint32_t ebp;
		uint16_t bp;
	};
	union {
		uintptr_t sp_ptr;
#ifdef TH_X64
		uint64_t rsp;
		uint8_t spl;
#endif
		uint32_t esp;
		uint16_t sp;
	};
	union {
		uintptr_t bx_ptr;
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
		uintptr_t dx_ptr;
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
		uintptr_t cx_ptr;
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
		uintptr_t ax_ptr;
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
	PVT_CODE = 15
};
typedef uint8_t patch_value_type_t;

// Description of a value specified by the options
typedef union {
	// Note: This is implemented as a struct within the main union
	// rather than as a field in an outer struct since the compiler
	// insisted on aligning things weirdly and taking up ~24 bytes of
	// data for a 16 byte struct, thus preventing XMM move optimizations.
	struct {
#if TH_X64
		unsigned char raw_bytes[24];
#else
		unsigned char raw_bytes[12];
#endif
		patch_value_type_t type;
		uint8_t merge_op; // Op values not exposed outside expression.cpp

		// Constpool fields
		uint8_t alignment;
		// Room for 1 more byte of flags or other data
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

} patch_val_t;

// Parses [expr], a string containing a [relative] or <absolute> patch value and writes it [out].
// Returns a pointer to the character following the parsed patch value or NULL on error.
// [regs] is either the current register structure if called from a breakpoint or null.
// [rel_source] is the address used when computing a relative value.
THCRAP_API const char* get_patch_value(const char* expr, patch_val_t* out, x86_reg_t* regs, uintptr_t rel_source, HMODULE hMod);

typedef enum {
	VECT_NONE		= 0,
	VECT_MMX		= 1 << 0,
	VECT_3DNOW		= 1 << 1,
	VECT_SSE		= 1 << 2,
	VECT_SSE2		= 1 << 3,
	VECT_SSE3		= 1 << 4,
	VECT_SSSE3		= 1 << 5,
	VECT_SSE4A		= 1 << 6,
	VECT_SSE41		= 1 << 7,
	VECT_SSE42		= 1 << 8,
	VECT_AVX		= 1 << 9,
	VECT_XOP		= 1 << 10,
	VECT_AVX2		= 1 << 11,
	VECT_AVX512A	= 1 << 12, // F, CD
	VECT_AVX512B	= 1 << 13, // VL, DQ, BW
	VECT_AVX512C	= 1 << 14, // IFMA, VBMI
	VECT_AVX512D	= 1 << 15, // VPOPCNTDQ, VNNI, VBMI2, BITALG
	VECT_AVX101		= 1 << 16,
	VECT_AVX102		= 1 << 17
} vector_tier_t;

typedef enum {
	VECW_NONE = 0,
	VECW_64 = 1,
	VECW_128 = 2,
	VECW_256 = 3,
	VECW_512 = 4
} vector_width_t;

bool CPU_Supports_SHA(void);
bool CPU_FDP_ErrorOnly(void);
bool CPU_FCS_FDS_Deprecated(void);
THCRAP_API bool OS_is_wine(void);
bool CPU_Supports_LMLSAHF(void);

// Returns a mask of the vector features enabled on the CPU
THCRAP_API vector_tier_t CPU_Vector_Features();

void DisableCodecaveNotFoundWarning(bool state);

// Returns a pointer to the register [regname] in [regs]. [endptr] behaves
// like the endptr parameter of strtol(), and can be a nullptr if not needed.
THCRAP_INTERNAL_API uint32_t* reg(x86_reg_t *regs, const char *regname, const char **endptr);

// Parses [expr], a string containing an expression terminated by [end].
// Returns a pointer to the character following the parsed expression or NULL on error.
// [regs] is either the current register structure if called from a breakpoint or null.
// [rel_source] is the address used when computing a relative value.
THCRAP_API const char* TH_FASTCALL eval_expr(const char* expr, char end, size_t* out, x86_reg_t* regs, uintptr_t rel_source, HMODULE hMod);

void patch_val_set_op(const char* op_str, patch_val_t* Val);

patch_val_t patch_val_op_str(const char* op_str, patch_val_t Val1, patch_val_t Val2);

/**
  * Returns the numeric value of a stringified address at the machine's word
  * size. The following string prefixes are supported:
  *
  *	- "0x": Hexadecimal, as expected.
  *	- "Rx": Hexadecimal value relative to the base address of the module given in hMod.
  *	        If hMod is NULL, the main module of the current process is used.
  * - "0b": Binary value
  * - "Rb": Binary version of "Rx".
  * - "0": Decimal, since accidental octal bugs are evil.
  * - "R": Decimal version of "Rx".
  */
#ifdef __cplusplus
extern "C" {
#endif
#if TH_X86
	extern uint64_t TH_FASTCALL str_to_addr_impl(uintptr_t base_addr, const char* str);
#else
	typedef struct {
		__m128i value;
		__m128i ptr;
	} str_to_addr_ret;
	extern str_to_addr_ret TH_VECTORCALL str_to_addr_impl(uintptr_t base_addr, const char* str);
#endif
#ifdef __cplusplus
}

extern "C++" {
template <typename T>
static TH_FORCEINLINE size_t str_to_addr(const char* str, const char*& end_str, T base_addr) {
#if TH_X86
	uint64_t temp = str_to_addr_impl(base_addr ? (uintptr_t)base_addr : CurrentImageBase, str);
	end_str = (const char*)(temp >> 32);
	return (size_t)temp;
#else
	str_to_addr_ret temp = str_to_addr_impl(base_addr ? (uintptr_t)base_addr : CurrentImageBase, str);
	size_t ret = *(size_t*)&temp.value;
	end_str = *(const char**)&temp.ptr;
	return ret;
#endif
}
template <typename T>
static TH_FORCEINLINE size_t str_to_addr(const char* str, T base_addr) {
	return (size_t)((uint64_t(TH_FASTCALL*)(uintptr_t, const char*))str_to_addr_impl)(base_addr ? (uintptr_t)base_addr : CurrentImageBase, str);
}
}
#endif
