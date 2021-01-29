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
enum value_type_t : uint8_t {
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
	VT_STRING,
	//VT_CODE
};

struct value_t {
	value_type_t type = VT_NONE;
	size_t size;
	union {
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
		unsigned char byte_array[8];
		const char* str;
	};

	value_t() {}

	value_t(void* in) {
		if (in) {
			p = (uintptr_t)in;
			type = VT_DWORD;
			size = sizeof(uintptr_t);
		} else {
			type = VT_NONE;
			size = 0;
		}
	}
	value_t(bool in) { b = in; type = VT_BYTE; size = sizeof(uint8_t); }
	value_t(uint8_t in) { b = in; type = VT_BYTE; size = sizeof(uint8_t); }
	value_t(int8_t in) { sb = in; type = VT_SBYTE; size = sizeof(int8_t); }
	value_t(uint16_t in) { w = in; type = VT_WORD; size = sizeof(uint16_t); }
	value_t(int16_t in) { sw = in; type = VT_SWORD; size = sizeof(int16_t); }
	value_t(uint32_t in) { i = in; type = VT_DWORD; size = sizeof(uint32_t); }
	value_t(int32_t in) { si = in; type = VT_SDWORD; size = sizeof(int32_t); }
	value_t(uint64_t in) { q = in; type = VT_QWORD; size = sizeof(uint64_t); }
	value_t(int64_t in) { sq = in; type = VT_SQWORD; size = sizeof(int64_t); }
	value_t(float in) { f = in; type = VT_FLOAT; size = sizeof(float); }
	value_t(double in) { d = in; type = VT_DOUBLE; size = sizeof(double); }
	value_t(const char* in) { str = in; type = VT_STRING; size = sizeof(const char*); }
	//value_t(const char* in, size_t size) { str = in; type = VT_CODE; size = size; }
};

const char* get_patch_value(const char* expr, value_t* out, x86_reg_t* regs, size_t rel_source);
#endif

void DisableCodecaveNotFoundWarning(bool state);

// Returns a pointer to the register [regname] in [regs]. [endptr] behaves
// like the endptr parameter of strtol(), and can be a nullptr if not needed.
size_t* reg(x86_reg_t *regs, const char *regname, const char **endptr);

const char* parse_brackets(const char* str, char opening);

bool CheckForType(const char * *const expr, size_t *const expr_len, CastType *const out);

//const char* consume_value(const char* expr, const char end, size_t *const out, const x86_reg_t *const regs, const size_t rel_source);
const char* __fastcall eval_expr(const char* expr, char end, size_t* out, x86_reg_t* regs, size_t rel_source);
