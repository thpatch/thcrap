/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Expression parsing
  */

#pragma once

// Register structure in PUSHAD+PUSHFD order at the beginning of a function
typedef struct {
	size_t flags;
	size_t edi;
	size_t esi;
	size_t ebp;
	size_t esp;
	size_t ebx;
	size_t edx;
	size_t ecx;
	size_t eax;
	size_t retaddr;
} x86_reg_t;

enum {
	invalid_cast = -1,
	no_cast = 0,
	T8_ptr = 1,
	u8_cast = 1,
	T16_ptr = 2,
	u16_cast = 2,
	T32_ptr = 4,
	u32_cast = 4,
	T64_ptr = 8,
	u64_cast = 8,
	b_cast,
	i8_cast,
	i16_cast,
	i32_cast,
	i64_cast,
	f32_cast,
	f64_cast
};
typedef uint8_t CastType;

#ifdef __cplusplus
enum value_type_t {
	VT_NONE = 0,
	VT_BYTE,
	VT_SBYTE,
	VT_WORD,
	VT_SWORD,
	VT_DWORD,
	VT_SDWORD,
	VT_QWORD,
	VT_SQWORD,
	VT_FLOAT,
	VT_DOUBLE,
	VT_XMMWORD
};

struct value_t {
	value_type_t type = VT_NONE;
	union {
		unsigned char b;
		signed char sb;
		unsigned short w;
		signed short sw;
		unsigned int i;
		signed int si;
		unsigned long long q;
		signed long long sq;
		uintptr_t p;
		float f;
		double d;
		unsigned char byte_array[8];
	};

	value_t() {}

	value_t(void* in) {
		if (in) {
			p = (uintptr_t)in;
			type = VT_DWORD;
		} else {
			type = VT_NONE;
		}
	}
	value_t(bool in) { b = in; type = VT_BYTE; }
	value_t(unsigned char in) { b = in; type = VT_BYTE; }
	value_t(signed char in) { sb = in; type = VT_SBYTE; }
	value_t(unsigned short in) { w = in; type = VT_WORD; }
	value_t(signed short in) { sw = in; type = VT_SWORD; }
	value_t(unsigned int in) { i = in; type = VT_DWORD; }
	value_t(signed int in) { si = in; type = VT_SDWORD; }
	value_t(unsigned long in) { i = in; type = VT_DWORD; }
	value_t(signed long in) { si = in; type = VT_SDWORD; }
	value_t(unsigned long long in) { q = in; type = VT_QWORD; }
	value_t(signed long long in) { sq = in; type = VT_SQWORD; }
	value_t(float in) { f = in; type = VT_FLOAT; }
	value_t(double in) { d = in; type = VT_DOUBLE; }

	size_t size() {
		switch (type) {
			case VT_NONE:
				return 0;
			case VT_BYTE: case VT_SBYTE:
				return 1;
			case VT_WORD: case VT_SWORD:
				return 2;
			case VT_DWORD: case VT_SDWORD: case VT_FLOAT:
				return 4;
			case VT_QWORD: case VT_SQWORD: case VT_DOUBLE:
				return 8;
			case VT_XMMWORD:
				return 16;
		}
	}
};

const char* get_patch_value(const char* expr, value_t* out, x86_reg_t* regs, size_t rel_source);
#endif

// Returns a pointer to the register [regname] in [regs]. [endptr] behaves
// like the endptr parameter of strtol(), and can be a nullptr if not needed.
size_t* reg(x86_reg_t *regs, const char *regname, const char **endptr);

const char* parse_brackets(const char* str, char opening);

bool CheckForType(const char * *const expr, size_t *const expr_len, CastType *const out);

//const char* consume_value(const char* expr, const char end, size_t *const out, const x86_reg_t *const regs, const size_t rel_source);
const char* eval_expr(const char* expr, size_t* out, char end, x86_reg_t* regs, size_t rel_source);
