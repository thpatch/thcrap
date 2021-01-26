/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Expression parsing
  */

#include "thcrap.h"
#include <intrin.h>

//#define EnableExpressionLogging

#ifdef EnableExpressionLogging
#define ExpressionLogging(...) log_printf(__VA_ARGS__)
#else
#define ExpressionLogging(...)
#endif

enum ManufacturerID : int8_t {
	Intel = -1,
	AMD = 0,
	Unknown = 1
};

struct CPUID_Data_t {
	ManufacturerID Manufacturer = Unknown;
	struct {
		bool HasCMPXCHG8 = false;
		bool HasCMOV = false;
		bool HasMMX = false;
		bool HasFXSAVE = false;
		bool HasSSE = false;
		bool HasSSE2 = false;
		bool HasSSE3 = false;
		bool HasPCLMULQDQ = false;
		bool HasSSSE3 = false;
		bool HasFMA = false;
		bool HasCMPXCHG16B = false;
		bool HasSSE41 = false;
		bool HasSSE42 = false;
		bool HasMOVBE = false;
		bool HasPOPCNT = false;
		bool HasAVX = false;
		bool HasF16C = false;
		bool HasBMI1 = false;
		bool HasAVX2 = false;
		bool HasBMI2 = false;
		bool HasERMS = false;
		bool HasAVX512F = false;
		bool HasAVX512DQ = false;
		bool HasADX = false;
		bool HasAVX512IFMA = false;
		bool HasAVX512PF = false;
		bool HasAVX512ER = false;
		bool HasAVX512CD = false;
		bool HasAVX512BW = false;
		bool HasAVX512VL = false;
		bool HasAVX512VBMI = false;
		bool HasAVX512VBMI2 = false;
		bool HasGFNI = false;
		bool HasVPCLMULQDQ = false;
		bool HasAVX512VNNI = false;
		bool HasAVX512BITALG = false;
		bool HasAVX512VPOPCNTDQ = false;
		bool HasAVX5124VNNIW = false;
		bool HasAVX5124FMAPS = false;
		bool HasFSRM = false;
		bool HasAVX512VP2I = false;
		bool HasAVX512BF16 = false;
		bool HasMMXEXT = false;
		bool Has3DNOWEXT = false;
		bool Has3DNOW = false;
		bool HasABM = false;
		bool HasSSE4A = false;
		bool HasXOP = false;
		bool HasFMA4 = false;
		bool HasTBM = false;
	};
	CPUID_Data_t(void) {
		int data[4];
		__cpuid(data, 0);
		uint32_t highest_leaf = data[0];
		if (data[1] == TextInt('G', 'e', 'n', 'u') &&
			data[3] == TextInt('i', 'n', 'e', 'I') &&
			data[2] == TextInt('n', 't', 'e', 'l')) {
				Manufacturer = Intel;
		}
		else if (data[1] == TextInt('A', 'u', 't', 'h') &&
				 data[3] == TextInt('e', 'n', 't', 'i') &&
				 data[2] == TextInt('c', 'A', 'M', 'D')) {
					Manufacturer = AMD;
		}
		else {
			Manufacturer = Unknown;
		}
		if (highest_leaf >= 1) {
			__cpuid(data, 1);
			HasCMPXCHG8			= data[3] & 1 << 8;
			HasCMOV				= data[3] & 1 << 15;
			HasMMX				= data[3] & 1 << 23;
			HasFXSAVE			= data[3] & 1 << 24;
			HasSSE				= data[3] & 1 << 25;
			HasSSE2				= data[3] & 1 << 26;
			HasSSE3				= data[2] & 1;
			HasPCLMULQDQ		= data[2] & 1 << 1;
			HasSSSE3			= data[2] & 1 << 9;
			HasFMA				= data[2] & 1 << 12;
			HasCMPXCHG16B		= data[2] & 1 << 13;
			HasSSE41			= data[2] & 1 << 19;
			HasSSE42			= data[2] & 1 << 20;
			HasMOVBE			= data[2] & 1 << 22;
			HasPOPCNT			= data[2] & 1 << 23;
			HasAVX				= data[2] & 1 << 28;
			HasF16C				= data[2] & 1 << 29;
		}
		if (highest_leaf >= 7) {
			__cpuidex(data, 7, 0);
			HasBMI1				= data[1] & 1 << 3;
			HasAVX2				= data[1] & 1 << 4;
			HasERMS				= data[1] & 1 << 9;
			HasAVX512F			= data[1] & 1 << 16;
			HasAVX512DQ			= data[1] & 1 << 17;
			HasADX				= data[1] & 1 << 19;
			HasAVX512IFMA		= data[1] & 1 << 21;
			HasAVX512PF			= data[1] & 1 << 26;
			HasAVX512ER			= data[1] & 1 << 27;
			HasAVX512CD			= data[1] & 1 << 28;
			HasAVX512BW			= data[1] & 1 << 30;
			HasAVX512VL			= data[1] & 1 << 31;
			HasAVX512VBMI		= data[2] & 1 << 1;
			HasAVX512VBMI2		= data[2] & 1 << 6;
			HasGFNI				= data[2] & 1 << 8;
			HasVPCLMULQDQ		= data[2] & 1 << 10;
			HasAVX512VNNI		= data[2] & 1 << 11;
			HasAVX512BITALG		= data[2] & 1 << 12;
			HasAVX512VPOPCNTDQ	= data[2] & 1 << 14;
			HasAVX5124VNNIW		= data[3] & 1 << 2;
			HasAVX5124FMAPS		= data[3] & 1 << 3;
			HasFSRM				= data[3] & 1 << 4;
			HasAVX512VP2I		= data[3] & 1 << 8;
			__cpuidex(data, 7, 1);
			HasAVX512BF16		= data[0] & 1 << 5;
		}
		if (highest_leaf >= 2) {
			__cpuid(data, 0x80000000);
			highest_leaf = data[0];
			if (highest_leaf >= 0x80000001) {
				HasMMXEXT		= data[3] & 1 << 22;
				Has3DNOWEXT		= data[3] & 1 << 30;
				Has3DNOW		= data[3] & 1 << 31;
				HasABM			= data[2] & 1 << 5;
				HasSSE4A		= data[2] & 1 << 6;
				HasXOP			= data[2] & 1 << 1;
				HasFMA4			= data[2] & 1 << 16;
				HasTBM			= data[2] & 1 << 21;
			}
		}
	}
};

static const CPUID_Data_t CPUID_Data;

#define breakpoint_test_var data_refs->regs
#define is_breakpoint (breakpoint_test_var)
#define is_binhack (!breakpoint_test_var)

#define WarnOnce(warning) do {\
	static bool AlreadyDisplayedWarning = false;\
	if (!AlreadyDisplayedWarning) { \
		warning; \
		AlreadyDisplayedWarning = true; \
	}\
} while (0)

static __declspec(noinline) void IncDecWarningMessage(void) {
	WarnOnce(log_printf("EXPRESSION WARNING 0: Prefix increment and decrement operators do not currently function as expected because it is not possible to modify the value of an option in an expression. These operators only function to add one to a value, but do not actually modify it.\n"));
}

static __declspec(noinline) void AssignmentWarningMessage(void) {
	WarnOnce(log_printf("EXPRESSION WARNING 1: Assignment operators do not currently function as expected because it is not possible to modify the value of an option in an expression. These operators are only included for future compatibility and operator precedence reasons.\n"));
}

static __declspec(noinline) void PatchValueWarningMessage(const char *const name) {
	log_printf("EXPRESSION WARNING 2: Unknown patch value type \"%s\", using value 0\n", name);
}

// The whole reason for having this is so that the log doesn't
// flood with warnings when calculating codecave sizes since
// their addresses haven't been found/recorded yet
static bool DisableCodecaveNotFound = false;
void DisableCodecaveNotFoundWarning(bool state) {
	DisableCodecaveNotFound = state;
}
static __declspec(noinline) void CodecaveNotFoundWarningMessage(const char *const name) {
	if (!DisableCodecaveNotFound) {
		log_printf("EXPRESSION WARNING 3: Codecave \"%s\" not found\n", name);
	}
}

static __declspec(noinline) void PostIncDecWarningMessage(void) {
	WarnOnce(log_printf("EXPRESSION WARNING 4: Postfix increment and decrement operators do not currently function as expected because it is not possible to modify the value of an option in an expression. These operators do nothing and are only included for future compatibility and operator precedence reasons.\n"));
}

static __declspec(noinline) void InvalidCPUFeatureWarningMessage(const char *const name) {
	log_printf("EXPRESSION WARNING 5: Unknown CPU feature \"%s\"! Assuming feature is present and returning 1...\n");
}

static __declspec(noinline) void ExpressionErrorMessage(void) {
	log_printf("EXPRESSION ERROR: Error parsing expression!\n");
}

static __declspec(noinline) void GroupingBracketErrorMessage(void) {
	log_printf("EXPRESSION ERROR 0: Unmatched grouping brackets\n");
}

static __declspec(noinline) void ValueBracketErrorMessage(void) {
	log_printf("EXPRESSION ERROR 1: Unmatched patch value brackets\n");
}

static __declspec(noinline) void BadCharacterErrorMessage(void) {
	log_printf("EXPRESSION ERROR 2: Unknown character\n");
}

static __declspec(noinline) void OptionNotFoundErrorMessage(const char *const name) {
	log_printf("EXPRESSION ERROR 3: Option \"%s\" not found\n", name);
}

static __declspec(noinline) void InvalidValueErrorMessage(const char *const str) {
	log_printf("EXPRESSION ERROR 4: Invalid value \"%s\"\n", str);
}

typedef uint8_t op_t;

typedef struct {
	const x86_reg_t *const regs;
	const size_t rel_source;
} StackSaver;

static const char* __fastcall eval_expr_new_impl(const char* expr, const char end, size_t *const out, const op_t start_op, const size_t start_value, const StackSaver *const data_refs);
static const char* __fastcall consume_value_impl(const char* expr, const char end, size_t *const out, const StackSaver *const data_refs);

size_t* reg(x86_reg_t *regs, const char *regname, const char **endptr) {
	uint32_t cmp;

	if (!regs || !regname || !regname[0] || !regname[1] || !regname[2]) {
		return NULL;
	}
	memcpy(&cmp, regname, 3);
	cmp &= 0x00FFFFFF;
	strlwr((char *)&cmp);

	if (endptr) {
		*endptr = regname + 3;
	}
	switch (cmp) {
		case TextInt('e', 'a', 'x'): return &regs->eax;
		case TextInt('e', 'c', 'x'): return &regs->ecx;
		case TextInt('e', 'd', 'x'): return &regs->edx;
		case TextInt('e', 'b', 'x'): return &regs->ebx;
		case TextInt('e', 's', 'p'): return &regs->esp;
		case TextInt('e', 'b', 'p'): return &regs->ebp;
		case TextInt('e', 's', 'i'): return &regs->esi;
		case TextInt('e', 'd', 'i'): return &regs->edi;
	}
	if (endptr) {
		*endptr = regname;
	}
	return NULL;
}

#define PreventOverlap(val) ((val) + (val))

#pragma warning(push)
// Intentional overflow/wraparound on some values,
// particularly variants of "or". Value only needs
// to be unique and easily calculated.
#pragma warning(disable : 4309 4369)
enum : uint8_t {
	NoOp = 0,
	StartNoOp = 1,
	EndGroupOp = 2,
	StandaloneTernaryEnd = 3,
	BadBrackets = 4,
	Power = '*' + '*',
	Multiply = '*',
	Divide = '/',
	Modulo = '%',
	Add = '+',
	Subtract = '-',
	ArithmeticLeftShift = '<' + '<',
	ArithmeticRightShift = '>' + '>',
	LogicalLeftShift = '<' + '<' + '<',
	LogicalRightShift = '>' + '>' + '>',
	CircularLeftShift = PreventOverlap('r') + '<' + '<',
	CircularLeftShiftB = PreventOverlap('R') + '<' + '<',
	CircularRightShift = PreventOverlap('r') + '>' + '>',
	CircularRightShiftB = PreventOverlap('R') + '>' + '>',
	ThreeWay = '<' + '=' + '>',
	Less = '<',
	LessEqual = '<' + '=',
	Greater = '>',
	GreaterEqual = '>' + '=',
	Equal = '=' + '=',
	NotEqual = PreventOverlap('!') + '=',
	BitwiseAnd = PreventOverlap('&'),
	BitwiseNand = '~' + PreventOverlap('&'),
	BitwiseXor = '^',
	BitwiseXnor = '~' + '^',
	BitwiseOr = PreventOverlap('|'),
	BitwiseNor = '~' + PreventOverlap('|'),
	LogicalAnd = PreventOverlap('&') + PreventOverlap('&'),
	LogicalNand = PreventOverlap('!') + PreventOverlap('&') + PreventOverlap('&'),
	LogicalXor = '^' + '^',
	LogicalXnor = PreventOverlap('!') + '^' + '^',
	LogicalOr = PreventOverlap('|') + PreventOverlap('|'),
	LogicalNor = PreventOverlap('!') + PreventOverlap('|') + PreventOverlap('|'),
	TernaryConditional = PreventOverlap('?'),
	Elvis = PreventOverlap('?') + ':',
	NullCoalescing = PreventOverlap('?') + PreventOverlap('?'),
	Assign = '=',
	AddAssign = '+' + '=',
	SubtractAssign = '-' + '=',
	MultiplyAssign = '*' + '=',
	DivideAssign = '/' + '=',
	ModuloAssign = '%' + '=',
	ArithmeticLeftShiftAssign = '<' + '<' + '=',
	ArithmeticRightShiftAssign = '>' + '>' + '=',
	LogicalLeftShiftAssign = '<' + '<' + '<' + '=',
	LogicalRightShiftAssign = '>' + '>' + '>' + '=',
	CircularLeftShiftAssign = PreventOverlap('r') + '<' + '<' + '=',
	CircularLeftShiftAssignB = PreventOverlap('R') + '<' + '<' + '=',
	CircularRightShiftAssign = PreventOverlap('r') + '>' + '>' + '=',
	CircularRightShiftAssignB = PreventOverlap('R') + '>' + '>' + '=',
	AndAssign = PreventOverlap('&') + '=',
	NandAssign = '~' + PreventOverlap('&') + '=',
	XorAssign = '^' + '=',
	XnorAssign = '~' + '^' + '=',
	OrAssign = PreventOverlap('|') + '=',
	NorAssign = '~' + PreventOverlap('|') + '=',
	NullCoalescingAssign = PreventOverlap('?') + PreventOverlap('?') + '=',
	Comma = ',',
	Gomma = ';'
};
#pragma warning(pop)

enum : uint8_t {
	LeftAssociative = 0,
	RightAssociative = 1
};

struct OpData_t {
	uint8_t Precedence[256] = { 0 };
	uint8_t Associativity[256] = { 0 };
	constexpr OpData_t(void) {
		Precedence[StartNoOp] = UINT8_MAX;
		Associativity[StartNoOp] = LeftAssociative;
#define POWER_PRECEDENCE 19
#define POWER_ASSOCIATIVITY LeftAssociative
		Precedence[Power] = POWER_PRECEDENCE;
		Associativity[Power] = POWER_ASSOCIATIVITY;
#define MULTIPLY_PRECEDENCE 17
#define MULTIPLY_ASSOCIATIVITY LeftAssociative
		Precedence[Multiply] = Precedence[Divide] = Precedence[Modulo] = MULTIPLY_PRECEDENCE;
		Associativity[Multiply] = Associativity[Divide] = Associativity[Modulo] = MULTIPLY_ASSOCIATIVITY;
#define ADD_PRECEDENCE 16
#define ADD_ASSOCIATIVITY LeftAssociative
		Precedence[Add] = Precedence[Subtract] = ADD_PRECEDENCE;
		Associativity[Add] = Associativity[Subtract] = ADD_ASSOCIATIVITY;
#define SHIFT_PRECEDENCE 15
#define SHIFT_ASSOCIATIVITY LeftAssociative
		Precedence[LogicalLeftShift] = Precedence[LogicalRightShift] = Precedence[ArithmeticLeftShift] = Precedence[ArithmeticRightShift] = Precedence[CircularLeftShift] = Precedence[CircularLeftShiftB] = Precedence[CircularRightShift] = Precedence[CircularRightShiftB] = SHIFT_PRECEDENCE;
		Associativity[LogicalLeftShift] = Associativity[LogicalRightShift] = Associativity[ArithmeticLeftShift] = Associativity[ArithmeticRightShift] = Associativity[CircularLeftShift] = Associativity[CircularLeftShiftB] = Associativity[CircularRightShift] = Associativity[CircularRightShiftB] = SHIFT_ASSOCIATIVITY;
#define COMPARE_PRECEDENCE 14
#define COMPARE_ASSOCIATIVITY LeftAssociative
		Precedence[Less] = Precedence[LessEqual] = Precedence[Greater] = Precedence[GreaterEqual] = COMPARE_PRECEDENCE;
		Associativity[Less] = Associativity[LessEqual] = Associativity[Greater] = Associativity[GreaterEqual] = COMPARE_ASSOCIATIVITY;
#define EQUALITY_PRECEDENCE 13
#define EQUALITY_ASSOCIATIVITY LeftAssociative
		Precedence[Equal] = Precedence[NotEqual] = EQUALITY_PRECEDENCE;
		Associativity[Equal] = Associativity[NotEqual] = EQUALITY_ASSOCIATIVITY;
#define THREEWAY_PRECEDENCE 12
#define THREEWAY_ASSOCIATIVITY LeftAssociative
		Precedence[ThreeWay] = THREEWAY_PRECEDENCE;
		Associativity[ThreeWay] = THREEWAY_ASSOCIATIVITY;
#define BITAND_PRECEDENCE 11
#define BITAND_ASSOCIATIVITY LeftAssociative
		Precedence[BitwiseAnd] = Precedence[BitwiseNand] = BITAND_PRECEDENCE;
		Associativity[BitwiseAnd] = Associativity[BitwiseNand] = BITAND_ASSOCIATIVITY;
#define BITXOR_PRECEDENCE 10
#define BITXOR_ASSOCIATIVITY LeftAssociative
		Precedence[BitwiseXor] = Precedence[BitwiseXnor] = BITXOR_PRECEDENCE;
		Associativity[BitwiseXor] = Associativity[BitwiseXnor] = BITXOR_ASSOCIATIVITY;
#define BITOR_PRECEDENCE 9
#define BITOR_ASSOCIATIVITY LeftAssociative
		Precedence[BitwiseOr] = Precedence[BitwiseNor] = BITOR_PRECEDENCE;
		Associativity[BitwiseOr] = Associativity[BitwiseNor] = BITOR_ASSOCIATIVITY;
#define AND_PRECEDENCE 8
#define AND_ASSOCIATIVITY LeftAssociative
		Precedence[LogicalAnd] = Precedence[LogicalNand] = AND_PRECEDENCE;
		Associativity[LogicalAnd] = Associativity[LogicalNand] = AND_ASSOCIATIVITY;
#define XOR_PRECEDENCE 7
#define XOR_ASSOCIATIVITY LeftAssociative
		Precedence[LogicalXor] = Precedence[LogicalXnor] = XOR_PRECEDENCE;
		Associativity[LogicalXor] = Associativity[LogicalXnor] = XOR_ASSOCIATIVITY;
#define OR_PRECEDENCE 6
#define OR_ASSOCIATIVITY LeftAssociative
		Precedence[LogicalOr] = Precedence[LogicalNor] = OR_PRECEDENCE;
		Associativity[LogicalOr] = Associativity[LogicalNor] = OR_ASSOCIATIVITY;
#define NULLC_PRECEDENCE 5
#define NULLC_ASSOCIATIVITY RightAssociative
		Precedence[NullCoalescing] = NULLC_PRECEDENCE;
		Associativity[NullCoalescing] = NULLC_ASSOCIATIVITY;
#define TERNARY_PRECEDENCE 4
#define TERNARY_ASSOCIATIVITY RightAssociative
		Precedence[TernaryConditional] = Precedence[Elvis] = TERNARY_PRECEDENCE;
		Associativity[TernaryConditional] = Associativity[Elvis] = TERNARY_ASSOCIATIVITY;
#define ASSIGN_PRECEDENCE 3
#define ASSIGN_ASSOCIATIVITY RightAssociative
		Precedence[Assign] = Precedence[AddAssign] = Precedence[SubtractAssign] = Precedence[MultiplyAssign] = Precedence[DivideAssign] = Precedence[ModuloAssign] = Precedence[LogicalLeftShiftAssign] = Precedence[LogicalRightShiftAssign] = Precedence[ArithmeticLeftShiftAssign] = Precedence[ArithmeticRightShiftAssign] = Precedence[CircularLeftShiftAssign] = Precedence[CircularLeftShiftAssignB] = Precedence[CircularRightShiftAssign] = Precedence[CircularRightShiftAssignB] = Precedence[AndAssign] = Precedence[OrAssign] = Precedence[XorAssign] = Precedence[NandAssign] = Precedence[XnorAssign] = Precedence[NorAssign] = Precedence[NullCoalescingAssign] = ASSIGN_PRECEDENCE;
		Associativity[Assign] = Associativity[AddAssign] = Associativity[SubtractAssign] = Associativity[MultiplyAssign] = Associativity[DivideAssign] = Associativity[ModuloAssign] = Associativity[LogicalLeftShiftAssign] = Associativity[LogicalRightShiftAssign] = Associativity[ArithmeticLeftShiftAssign] = Associativity[ArithmeticRightShiftAssign] = Associativity[CircularLeftShiftAssign] = Associativity[CircularLeftShiftAssignB] = Associativity[CircularRightShiftAssign] = Associativity[CircularRightShiftAssignB] = Associativity[AndAssign] = Associativity[OrAssign] = Associativity[XorAssign] = Associativity[NandAssign] = Associativity[XnorAssign] = Associativity[NorAssign] = Associativity[NullCoalescingAssign] = ASSIGN_ASSOCIATIVITY;
#define COMMA_PRECEDENCE 2
#define COMMA_ASSOCIATIVITY LeftAssociative
		Precedence[Comma] = COMMA_PRECEDENCE;
		Associativity[Comma] = COMMA_ASSOCIATIVITY;
#define GOMMA_PRECEDENCE 1
#define GOMMA_ASSOCIATIVITY LeftAssociative
		Precedence[Gomma] = GOMMA_PRECEDENCE;
		Associativity[Gomma] = GOMMA_ASSOCIATIVITY;
#define NOOP_PRECEDENCE 0
#define NOOP_ASSOCIATIVITY LeftAssociative
		Precedence[NoOp] = Precedence[EndGroupOp] = NOOP_PRECEDENCE;
		Associativity[NoOp] = Associativity[EndGroupOp] = NOOP_ASSOCIATIVITY;
	}
};

static constexpr OpData_t OpData;

union CheapPack {
	const uint32_t in;
	struct {
		const char start;
		const char end;
	};
};

static __declspec(noinline) const char* find_matching_end(const char* str, const uint32_t delims_in) {
	const CheapPack delims = { delims_in };
	if (!str) return NULL;
	size_t depth = 0;
	while (str[0]) {
		if (str[0] == delims.start) {
			++depth;
		}
		else if (str[0] == delims.end) {
			--depth;
			if (!depth) {
				return str;
			}
		}
		++str;
	}
	return NULL;
};

const char* parse_brackets(const char* str, const char opening) {
	const char* str_ref = str - 1;
	int32_t paren_count = opening == '(';
	int32_t square_count = opening == '[';
	int32_t curly_count = opening == '{';
	int32_t angle_count = opening == '<';
	char c;
	while (c = *++str_ref) {
		paren_count += c == '(';
		paren_count -= c == ')';
		square_count += c == '[';
		square_count -= c == ']';
		curly_count += c == '{';
		curly_count -= c == '}';
		angle_count += c == '<';
		angle_count -= c == '>';
		if ((paren_count | square_count | curly_count | angle_count) == 0) {
			return str_ref;
		}
		if ((paren_count | square_count | curly_count | angle_count) < 0) {
			return str;
		}
	}
	return str;
}

static inline const char* __fastcall find_next_op_impl(const char *const expr, op_t *const out) {
	uint32_t c;
	const char* expr_ref = expr - 1;
	uint32_t temp;
	while (1) {
		switch (c = *(unsigned char*)++expr_ref) {
			case '\0':
				*out = c;
				return expr;
			case '(': case '[': case '{':
				*out = BadBrackets;
				return expr;
			case ')': case ']': case '}': case ':':
				*out = EndGroupOp;
				return expr_ref;
			case '~':
				switch (temp = expr_ref[1]) {
					case '&': case '|':
						temp += temp;
						[[fallthrough]];
					case '^':
						if (expr_ref[2] == '=') {
							*out = c + temp + '=';
							return expr_ref + 3;
						}
						else {
							*out = c + temp;
							return expr_ref + 2;
						}
				}
				continue;
			case '!':
				c += c;
				switch (temp = expr_ref[1]) {
					case '=':
						*out = c + temp;
						return expr_ref + 2;
					case '^':
						if (expr_ref[2] == temp) {
							*out = c + temp * 2;
							return expr_ref + 3;
						}
						break;
					case '&': case '|':
						if (expr_ref[2] == temp) {
							*out = c + temp * 4;
							return expr_ref + 3;
						}
				}
				continue;
			case '?':
				c += c;
				// All of these offset expr_ref by
				// 1 less than normal in order to
				// more easily process the operators.
				switch (expr_ref[1]) {
					case '?':
						if (expr_ref[2] == '=') {
							*out = c * 2 + '=';
							return expr_ref + 3;
						}
						else {
							*out = c * 2;
							return expr_ref + 2;
						}
					case ':':
						*out = c + ':';
						return expr_ref + 1;
					default:
						*out = c;
						return expr_ref;
				}
			case '<': case '>':
				if (expr_ref[1]) {
					if (expr_ref[2]) {
						temp = *(uint32_t*)expr_ref;
						// Yeah, the if chain is kind of ugly. However, this can't
						// optimize into a jump table and using a switch adds an
						// extra check for "higher than included" values, which are
						// impossible anyway since they already got filtered out.
						if (temp == TextInt('>', '>', '>', '=') || temp == TextInt('<', '<', '<', '=')) {
							*out = c * 3 + '=';
							return expr_ref + 4;
						}
						temp &= 0x00FFFFFF;
						if (temp == TextInt('<', '=', '>')) {
							*out = ThreeWay;
							return expr_ref + 3;
						}
						if (temp == TextInt('>', '>', '=') || temp == TextInt('<', '<', '=')) {
							*out = c * 2 + '=';
							return expr_ref + 3;
						}
						if (temp == TextInt('>', '>', '>') || temp == TextInt('<', '<', '<')) {
							*out = c * 3;
							return expr_ref + 3;
						}
						temp &= 0x0000FFFF;
					}
					else {
						temp = *(uint16_t*)expr_ref;
					}
					if (temp == TextInt('<', '=') || temp == TextInt('>', '=')) {
						*out = c + '=';
						return expr_ref + 2;
					}
					if (temp == TextInt('>', '>') || temp == TextInt('<', '<')) {
						*out = c * 2;
						return expr_ref + 2;
					}
				}
				*out = c;
				return expr_ref + 1;
			// No fallthrough
			case 'r': case 'R':
				if (expr_ref[1] && expr_ref[2]) {
					c += c;
					temp = *(uint32_t*)expr_ref;
					if (temp == TextInt('r', '>', '>', '=') || temp == TextInt('R', '>', '>', '=')) {
						*out = c + '>' + '>' + '=';
						return expr_ref + 4;
					}
					if (temp == TextInt('r', '<', '<', '=') || temp == TextInt('R', '<', '<', '=')) {
						*out = c + '<' + '<' + '=';
						return expr_ref + 4;
					}
					temp &= 0x00FFFFFF;
					if (temp == TextInt('r', '>', '>') || temp == TextInt('R', '>', '>')) {
						*out = c + '<' + '<';
						return expr_ref + 3;
					}
					if (temp == TextInt('r', '<', '<') || temp == TextInt('R', '<', '<')) {
						*out = c + '>' + '>';
						return expr_ref + 3;
					}
				}
				continue;
			// No fallthrough
			case '&': case '|':
				c += c;
				[[fallthrough]];
			case '+': case '-': case '*': case '/': case '%': case '=': case '^':
				if (expr_ref[1]) {
					temp = *(uint16_t*)expr_ref;
					if (temp == TextInt('=', '=') || temp == TextInt('&', '&') ||
						temp == TextInt('|', '|') || temp == TextInt('*', '*') ||
						temp == TextInt('^', '^')) {
							*out = c * 2;
							return expr_ref + 2;
					}
					if (temp == TextInt('|', '=') || temp == TextInt('^', '=') ||
						temp == TextInt('%', '=') || temp == TextInt('&', '=') ||
						temp == TextInt('*', '=') || temp == TextInt('+', '=') ||
						temp == TextInt('-', '=') || temp == TextInt('/', '=')) {
							*out = c + '=';
							return expr_ref + 2;
					}
				}
				[[fallthrough]];
			case ',': case ';':
				*out = c;
				return expr_ref + 1;
		}
	}
}

// Returns a string containing a textual representation of the operator
static inline const char *const PrintOp(const op_t op) {
	switch (op) {
		case StartNoOp: return "StartNoOp";
		case Power: return "**";
		case Multiply: return "*";
		case Divide: return "/";
		case Modulo: return "%";
		case Add: return "+";
		case Subtract: return "-";
		case ArithmeticLeftShift: return "<<";
		case ArithmeticRightShift: return ">>";
		case LogicalLeftShift: return "<<<";
		case LogicalRightShift: return ">>>";
		case CircularLeftShift:
		case CircularLeftShiftB: return "r<<";
		case CircularRightShift:
		case CircularRightShiftB: return "r>>";
		case BitwiseAnd: return "&";
		case BitwiseNand: return "~&";
		case BitwiseXor: return "^";
		case BitwiseXnor: return "~^";
		case BitwiseOr: return "|";
		case BitwiseNor: return "~|";
		case LogicalAnd: return "&&";
		case LogicalNand: return "!&&";
		case LogicalXor: return "^^";
		case LogicalXnor: return "!^^";
		case LogicalOr: return "||";
		case LogicalNor: return "!||";
		case Less: return "<";
		case LessEqual: return "<=";
		case Greater: return ">";
		case GreaterEqual: return ">=";
		case Equal: return "==";
		case NotEqual: return "!=";
		case ThreeWay: return "<=>";
		case TernaryConditional: return "?";
		case Elvis: return "?:";
		case NullCoalescing: return "??";
		case Assign: return "=";
		case AddAssign: return "+=";
		case SubtractAssign: return "-=";
		case MultiplyAssign: return "*=";
		case DivideAssign: return "/=";
		case ModuloAssign: return "%=";
		case ArithmeticLeftShiftAssign: return "<<=";
		case ArithmeticRightShiftAssign: return ">>=";
		case LogicalLeftShiftAssign: return "<<<=";
		case LogicalRightShiftAssign: return ">>>=";
		case CircularLeftShiftAssign:
		case CircularLeftShiftAssignB: return "r<<=";
		case CircularRightShiftAssign:
		case CircularRightShiftAssignB: return "r>>=";
		case AndAssign: return "&=";
		case XorAssign: return "^=";
		case OrAssign: return "|=";
		case NandAssign: return "~&=";
		case XnorAssign: return "~^=";
		case NorAssign: return "~|=";
		case NullCoalescingAssign: return "??=";
		case Comma: return ",";
		case Gomma: return ";";
		case NoOp: return "NoOp";
		case EndGroupOp: return "EndGroupNoOp";
		case StandaloneTernaryEnd: return "TernaryNoOp";
		case BadBrackets: return "BadBrackets";
		default: return "ERROR";
	}
}

enum : int8_t {
	HigherThanNext = 1,
	SameAsNext = 0,
	LowerThanNext = -1,
	LowerThanPrev = 1,
	SameAsPrev = 0,
	HigherThanPrev = -1
};

static inline int8_t CompareOpPrecedence(const op_t PrevOp, const op_t NextOp) {
	const uint8_t PrevPrecedence = OpData.Precedence[PrevOp];
	const uint8_t NextPrecedence = OpData.Precedence[NextOp];
	if (PrevPrecedence > NextPrecedence) {
		return HigherThanNext;
	}
	else if (PrevPrecedence < NextPrecedence) {
		return LowerThanNext;
	}
	else {
		return SameAsNext;
	}
}

// This is implemented here since VS insists on adding
// a bunch of baggage to ApplyOperator if it's inlined
static __declspec(noinline) size_t ApplyPower(const size_t value, const size_t arg) {
	return (size_t)pow(value, arg);
}

static __declspec(noinline) size_t ApplyOperator(const size_t value, const size_t arg, const op_t op) {
	switch (op) {
		case Power:
			return ApplyPower(value, arg);
		case MultiplyAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case Multiply:
			return value * arg;
		case DivideAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case Divide:
			return value / arg;
		case ModuloAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case Modulo:
			return value % arg;
		case AddAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case Add:
			return value + arg;
		case SubtractAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case Subtract:
			return value - arg;
		case LogicalLeftShiftAssign:
		case ArithmeticLeftShiftAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case LogicalLeftShift:
		case ArithmeticLeftShift:
			return value << arg;
		case LogicalRightShiftAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case LogicalRightShift:
			return value >> arg;
		case ArithmeticRightShiftAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case ArithmeticRightShift:
			return (uint32_t)((int32_t)value >> arg);
		case CircularLeftShiftAssign:
		case CircularLeftShiftAssignB:
			AssignmentWarningMessage();
			[[fallthrough]];
		case CircularLeftShift:
		case CircularLeftShiftB:
			return value << arg | value >> (sizeof(value) * CHAR_BIT - arg);
		case CircularRightShiftAssign:
		case CircularRightShiftAssignB:
			AssignmentWarningMessage();
			[[fallthrough]];
		case CircularRightShift:
		case CircularRightShiftB:
			return value >> arg | value << (sizeof(value) * CHAR_BIT - arg);
		case Less:
			return value < arg;
		case LessEqual:
			return value <= arg;
		case Greater:
			return value > arg;
		case GreaterEqual:
			return value >= arg;
		case Equal:
			return value == arg;
		case NotEqual:
			return value != arg;
		case ThreeWay:
			return value == arg ? 0 : value > arg ? 1 : -1;
		case AndAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case BitwiseAnd:
			return value & arg;
		case NandAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case BitwiseNand:
			return ~(value & arg);
		case XorAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case BitwiseXor:
			return value ^ arg;
		case XnorAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case BitwiseXnor:
			return ~(value ^ arg);
		case OrAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case BitwiseOr:
			return value | arg;
		case NorAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case BitwiseNor:
			return ~(value | arg);
		case LogicalAnd:
			return value && arg;
		case LogicalNand:
			return !(value && arg);
		case LogicalXor:
			return (bool)(value ^ arg);
		case LogicalXnor:
			return (bool)!(value ^ arg);
		case LogicalOr:
			return value || arg;
		case LogicalNor:
			return !(value || arg);
		case NullCoalescingAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case NullCoalescing:
			return value ? value : arg;
			break;
		case Assign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case Comma:
		case Gomma:
			//why tho
		case StartNoOp:
		case NoOp:
		case EndGroupOp:
		case StandaloneTernaryEnd:
		default:
			return arg;
	}
}

static __declspec(noinline) __declspec(safebuffers) value_t GetOptionValue(const char *const name, const size_t name_length) {
	const char *const name_buffer = strndup(name, name_length);
	ExpressionLogging("Option: \"%s\"\n", name_buffer);
	const patch_opt_val_t *const option = patch_opt_get(name_buffer);
	value_t ret = 0u;
	if (option) {
		switch (option->t) {
			case PATCH_OPT_VAL_BYTE: ret = option->val.byte; break;
			case PATCH_OPT_VAL_SBYTE: ret = option->val.sbyte; break;
			case PATCH_OPT_VAL_WORD: ret = option->val.word; break;
			case PATCH_OPT_VAL_SWORD: ret = option->val.sword; break;
			case PATCH_OPT_VAL_DWORD: ret = option->val.dword; break;
			case PATCH_OPT_VAL_SDWORD: ret = option->val.sdword; break;
			case PATCH_OPT_VAL_FLOAT: ret = option->val.f; break;
			case PATCH_OPT_VAL_DOUBLE: ret = option->val.d; break;
		}
	}
	else {
		OptionNotFoundErrorMessage(name_buffer);
		ret.type = VT_NONE;
	}
	free((void*)name_buffer);
	return ret;
}

static __declspec(noinline) value_t GetPatchTestValue(const char *const name, const size_t name_length) {
	const char *const name_buffer = strndup(name, name_length);
	ExpressionLogging("PatchTest: \"%s\"\n", name_buffer);
	const patch_opt_val_t *const patch_test = patch_opt_get(name_buffer);
	value_t ret = 0u;
	if (patch_test) {
		switch (patch_test->t) {
			case PATCH_OPT_VAL_BYTE: ret = patch_test->val.byte; break;
			case PATCH_OPT_VAL_SBYTE: ret = patch_test->val.sbyte; break;
			case PATCH_OPT_VAL_WORD: ret = patch_test->val.word; break;
			case PATCH_OPT_VAL_SWORD: ret = patch_test->val.sword; break;
			case PATCH_OPT_VAL_DWORD: ret = patch_test->val.dword; break;
			case PATCH_OPT_VAL_SDWORD: ret = patch_test->val.sdword; break;
			case PATCH_OPT_VAL_FLOAT: ret = patch_test->val.f; break;
			case PATCH_OPT_VAL_DOUBLE: ret = patch_test->val.d; break;
		}
	}
	else {
		ret.type = VT_NONE;
	}
	free((void*)name_buffer);
	return ret;
}

static __declspec(noinline) value_t GetCPUFeatureTest(const char *const name, const size_t name_length) {
	const char *const name_buffer = strndup(name, name_length);
	ExpressionLogging("CPUFeatureTest: \"%s\"\n", name_buffer);
	value_t ret = false;
	// Yuck
	switch (name_length) {
		case 15:
			if		(stricmp(name_buffer, "avx512vpopcntdq") == 0) ret.b = CPUID_Data.HasAVX512VPOPCNTDQ;
			else	goto InvalidCPUFeatureError;
			break;
		case 12:
			if		(stricmp(name_buffer, "avx512bitalg") == 0) ret.b = CPUID_Data.HasAVX512BITALG;
			else if (stricmp(name_buffer, "avx5124fmaps") == 0) ret.b = CPUID_Data.HasAVX5124FMAPS;
			else if (stricmp(name_buffer, "avx5124vnniw") == 0) ret.b = CPUID_Data.HasAVX5124VNNIW;
			else	goto InvalidCPUFeatureError;
			break;
		case 11:
			if		(stricmp(name_buffer, "avx512vbmi1") == 0) ret.b = CPUID_Data.HasAVX512VBMI;
			else if (stricmp(name_buffer, "avx512vbmi2") == 0) ret.b = CPUID_Data.HasAVX512VBMI2;
			else	goto InvalidCPUFeatureError;
			break;
		case 10:
			if		(stricmp(name_buffer, "cmpxchg16b") == 0) ret.b = CPUID_Data.HasCMPXCHG16B;
			else if (stricmp(name_buffer, "vpclmulqdq") == 0) ret.b = CPUID_Data.HasVPCLMULQDQ;
			else if (stricmp(name_buffer, "avx512ifma") == 0) ret.b = CPUID_Data.HasAVX512IFMA;
			else if (stricmp(name_buffer, "avx512vnni") == 0) ret.b = CPUID_Data.HasAVX512VNNI;
			else if (stricmp(name_buffer, "avx512vp2i") == 0) ret.b = CPUID_Data.HasAVX512VP2I;
			else if (stricmp(name_buffer, "avx512bf16") == 0) ret.b = CPUID_Data.HasAVX512BF16;
			else	goto InvalidCPUFeatureError;
			break;
		case 9:
			if		(stricmp(name_buffer, "pclmulqdq") == 0) ret.b = CPUID_Data.HasPCLMULQDQ;
			else	goto InvalidCPUFeatureError;
			break;
		case 8:
			if		(stricmp(name_buffer, "cmpxchg8") == 0) ret.b = CPUID_Data.HasCMPXCHG8;
			else if (stricmp(name_buffer, "avx512dq") == 0) ret.b = CPUID_Data.HasAVX512DQ;
			else if (stricmp(name_buffer, "avx512pf") == 0) ret.b = CPUID_Data.HasAVX512PF;
			else if (stricmp(name_buffer, "avx512er") == 0) ret.b = CPUID_Data.HasAVX512ER;
			else if (stricmp(name_buffer, "avx512cd") == 0) ret.b = CPUID_Data.HasAVX512CD;
			else if (stricmp(name_buffer, "avx512bw") == 0) ret.b = CPUID_Data.HasAVX512BW;
			else if (stricmp(name_buffer, "avx512vl") == 0) ret.b = CPUID_Data.HasAVX512VL;
			else if (stricmp(name_buffer, "3dnowext") == 0) ret.b = CPUID_Data.Has3DNOWEXT;
			else	goto InvalidCPUFeatureError;
			break;
		case 7:
			if		(stricmp(name_buffer, "avx512f") == 0) ret.b = CPUID_Data.HasAVX512F;
			else	goto InvalidCPUFeatureError;
			break;
		case 6:
			if		(stricmp(name_buffer, "popcnt") == 0) ret.b = CPUID_Data.HasPOPCNT;
			else if (stricmp(name_buffer, "fxsave") == 0) ret.b = CPUID_Data.HasFXSAVE;
			else if (stricmp(name_buffer, "mmxext") == 0) ret.b = CPUID_Data.HasMMXEXT;
			else	goto InvalidCPUFeatureError;
			break;
		case 5:
			if		(stricmp(name_buffer, "intel") == 0) ret.b = CPUID_Data.Manufacturer == Intel;
			else if (stricmp(name_buffer, "ssse3") == 0) ret.b = CPUID_Data.HasSSSE3;
			else if (stricmp(name_buffer, "sse41") == 0) ret.b = CPUID_Data.HasSSE41;
			else if (stricmp(name_buffer, "sse42") == 0) ret.b = CPUID_Data.HasSSE42;
			else if (stricmp(name_buffer, "sse4a") == 0) ret.b = CPUID_Data.HasSSE4A;
			else if (stricmp(name_buffer, "movbe") == 0) ret.b = CPUID_Data.HasMOVBE;
			else if (stricmp(name_buffer, "3dnow") == 0) ret.b = CPUID_Data.Has3DNOW;
			else	goto InvalidCPUFeatureError;
			break;
		case 4:
			if		(stricmp(name_buffer, "cmov") == 0) ret.b = CPUID_Data.HasCMOV;
			else if (stricmp(name_buffer, "sse2") == 0) ret.b = CPUID_Data.HasSSE2;
			else if (stricmp(name_buffer, "sse3") == 0) ret.b = CPUID_Data.HasSSE3;
			else if (stricmp(name_buffer, "avx2") == 0) ret.b = CPUID_Data.HasAVX2;
			else if (stricmp(name_buffer, "bmi1") == 0) ret.b = CPUID_Data.HasBMI1;
			else if (stricmp(name_buffer, "bmi2") == 0) ret.b = CPUID_Data.HasBMI2;
			else if (stricmp(name_buffer, "erms") == 0) ret.b = CPUID_Data.HasERMS;
			else if (stricmp(name_buffer, "fsrm") == 0) ret.b = CPUID_Data.HasFSRM;
			else if (stricmp(name_buffer, "f16c") == 0) ret.b = CPUID_Data.HasF16C;
			else if (stricmp(name_buffer, "gfni") == 0) ret.b = CPUID_Data.HasGFNI;
			else if (stricmp(name_buffer, "fma4") == 0) ret.b = CPUID_Data.HasFMA4;
			else	goto InvalidCPUFeatureError;
			break;
		case 3:
			if		(stricmp(name_buffer, "amd") == 0) ret.b = CPUID_Data.Manufacturer == AMD;
			else if (stricmp(name_buffer, "sse") == 0) ret.b = CPUID_Data.HasSSE;
			else if (stricmp(name_buffer, "fma") == 0) ret.b = CPUID_Data.HasFMA;
			else if (stricmp(name_buffer, "mmx") == 0) ret.b = CPUID_Data.HasMMX;
			else if (stricmp(name_buffer, "avx") == 0) ret.b = CPUID_Data.HasAVX;
			else if (stricmp(name_buffer, "adx") == 0) ret.b = CPUID_Data.HasADX;
			else if (stricmp(name_buffer, "abm") == 0) ret.b = CPUID_Data.HasABM;
			else if (stricmp(name_buffer, "xop") == 0) ret.b = CPUID_Data.HasXOP;
			else if (stricmp(name_buffer, "tbm") == 0) ret.b = CPUID_Data.HasTBM;
			else	goto InvalidCPUFeatureError;
			break;
		default:
InvalidCPUFeatureError:
			InvalidCPUFeatureWarningMessage(name_buffer);
	}
	free((void*)name_buffer);
	return ret;
}

static __declspec(noinline) value_t GetCodecaveAddress(const char *const name, const size_t name_length, const bool is_relative, const StackSaver *const data_refs) {
	char *const name_buffer = strndup(name, name_length);
	ExpressionLogging("CodecaveAddress: \"%s\"\n", name_buffer);
	char* user_offset_expr = strchr(name_buffer, '+');
	if (user_offset_expr) {
		// This prevents next read to name_buffer
		// from containing the offset value
		*user_offset_expr++ = '\0';
	}
	value_t ret;
	ret.type = VT_DWORD;
	ret.i = func_get(name_buffer);
	if (!ret.i) {
		CodecaveNotFoundWarningMessage(name_buffer);
	}
	else {
		if (user_offset_expr) {
			char* user_offset_expr_next;
			// Try a hex value by default for compatibility
			size_t user_offset_value = strtoul(user_offset_expr, &user_offset_expr_next, 16);
			// If a hex value doesn't work, try a subexpression
			if (user_offset_expr == user_offset_expr_next) {
				user_offset_expr_next = (char*)eval_expr_new_impl(user_offset_expr, '\0', &user_offset_value, StartNoOp, 0, data_refs);
				if (user_offset_expr == user_offset_expr_next) {
					ExpressionErrorMessage();
				}
			}
			if (user_offset_expr != user_offset_expr_next) {
				ret.i += user_offset_value;
			}
		}
		if (is_relative) {
			ret.i -= data_refs->rel_source + sizeof(void*);
		}
	}
	free(name_buffer);
	return ret;
}

static __declspec(noinline) value_t GetBPFuncOrRawAddress(const char *const name, const size_t name_length, const bool is_relative, const StackSaver *const data_refs) {
	const char *const name_buffer = strndup(name, name_length);
	ExpressionLogging("BPFuncOrRawAddress: \"%s\"\n", name_buffer);
	value_t ret;
	ret.type = VT_DWORD;
	ret.i = func_get(name_buffer);
	if (!ret.i) { // Will be null if the name was not a BP function
		const char *const name_buffer_next = eval_expr_new_impl(name_buffer, '\0', &ret.i, StartNoOp, 0, data_refs);
		if (name_buffer == name_buffer_next) {
			ExpressionErrorMessage();
			goto SkipRelative;
			// Ret would still be 0, so keep going
			// to free the name string and return 0
		}
	}
	if (is_relative) {
		ret.i -= data_refs->rel_source + sizeof(void*);
	}
SkipRelative:
	free((void*)name_buffer);
	return ret;
}

static __declspec(noinline) const char* get_patch_value_impl(const char* expr, value_t *const out, const StackSaver *const data_refs) {
	const char opening = expr[0];
	ExpressionLogging("Patch value opening char: \"%hhX\"\n", opening);
	bool is_relative = opening == '[';
	const char *const patch_val_end = find_matching_end(expr, { TextInt(opening, is_relative ? ']' : '>') });
	ExpressionLogging("Patch value end: \"%s\"\n", patch_val_end ? patch_val_end : "NULL");
	if (!patch_val_end) {
		//Bracket error
		return expr;
	}
	// Increment expr so that the comparisons
	// don't check the opening bracket
	++expr;
	// This would be a lot simpler if there were a variant of
	// patch_opt_get and func_get that took a length parameter
	if (strnicmp(expr, "codecave:", 9) == 0) {
		*out = GetCodecaveAddress(expr, PtrDiffStrlen(patch_val_end, expr), is_relative, data_refs);
	}
	else if (strnicmp(expr, "option:", 7) == 0) {
		expr += 7;
		*out = GetOptionValue(expr, PtrDiffStrlen(patch_val_end, expr));
	}
	else if (strnicmp(expr, "patch:", 6) == 0) {
		*out = GetPatchTestValue(expr, PtrDiffStrlen(patch_val_end, expr));
	}
	else if (strnicmp(expr, "cpuid:", 6) == 0) {
		expr += 6;
		*out = GetCPUFeatureTest(expr, PtrDiffStrlen(patch_val_end, expr));
	}
	else {
		*out = GetBPFuncOrRawAddress(expr, PtrDiffStrlen(patch_val_end, expr), is_relative, data_refs);
	}
	return patch_val_end + 1;
};

const char* get_patch_value(const char* expr, value_t* out, x86_reg_t* regs, size_t rel_source) {
	ExpressionLogging("START PATCH VALUE \"%s\"\n", expr);

	const StackSaver data_refs = { regs, rel_source };

	const char *const expr_next = get_patch_value_impl(expr, out, &data_refs);
	if (expr == expr_next) goto ExpressionError;
	ExpressionLogging(
		"END PATCH VALUE\n"
		"\t\t\t\t\t\t\t\t\t\t\t\t\tPatch value was: \"%s\"\n"
		"\t\t\t\t\t\t\t\t\t\t\t\t\tFINAL result was: %X / %d / %u\n"
		"Remaining after final: \"%s\"\n",
		expr, out->i, out->i, out->i, expr_next
	);
	return expr_next;

ExpressionError:
	ExpressionErrorMessage();
	out->type = VT_DWORD;
	out->i = 0;
	ExpressionLogging(
		"END PATCH VALUE WITH ERROR\n"
		"\t\t\t\t\t\t\t\t\t\t\t\t\tPatch value was: \"%s\"\n"
		"\t\t\t\t\t\t\t\t\t\t\t\t\tFINAL result was: %X / %d / %u\n"
		"Remaining after final: \"%s\"\n",
		expr, out->i, out->i, out->i, expr_next
	);
	return expr_next;
}

static inline const char* CheckCastType(const char* expr, uint8_t &out) {
	if (expr[0] && expr[1] && expr[2]) {
		uint32_t cast = *(uint32_t*)expr;
		ExpressionLogging("Cast debug: %X\n");
		if (cast == TextInt('I', '3', '2', ')') ||
			cast == TextInt('i', '3', '2', ')')) {
				out = i32_cast;
				return expr + 4;
		}
		if (cast == TextInt('U', '3', '2', ')') ||
			cast == TextInt('u', '3', '2', ')')) {
				out = u32_cast;
				return expr + 4;
		}
		if (cast == TextInt('F', '3', '2', ')') ||
			cast == TextInt('f', '3', '2', ')')) {
				out = f32_cast;
				return expr + 4;
		}
		if (cast == TextInt('I', '1', '6', ')') ||
			cast == TextInt('i', '3', '2', ')')) {
				out = i16_cast;
				return expr + 4;
		}
		if (cast == TextInt('U', '1', '6', ')') ||
			cast == TextInt('u', '1', '6', ')')) {
				out = u16_cast;
				return expr + 3;
		}
		cast &= 0x00FFFFFF;
		if (cast == TextInt('I', '8', ')') ||
			cast == TextInt('i', '8', ')')) {
				out = i8_cast;
				return expr + 3;
		}
		if (cast == TextInt('U', '8', ')') ||
			cast == TextInt('u', '8', ')')) {
				out = u8_cast;
				return expr + 3;
		}
	}
	return expr;
};

static __forceinline const char* is_reg_name(const char *const expr, const x86_reg_t *const regs, size_t &out) {
	if (!expr[0] || !expr[1] || !expr[2]) {
		return expr;
	}
	const char* ret = expr;
	bool deref = ret[0] != '&';
	if (!deref) {
		++ret;
	}
	// Since !expr_ref[2] already validated the string has at least
	// three non-null characters, there must also be at least one
	// more character because of the null terminator, thus it's fine
	// to just read a whole 4 bytes at once.
	ExpressionLogging("Reg test: \"%s\"\n", ret);
	uint32_t cmp = *(uint32_t*)ret & 0x00FFFFFF;
	strlwr((char*)&cmp);
	ExpressionLogging("Reg string: \"%s\"\n", &cmp);
	switch (cmp) {
		default:
			return expr;
		case TextInt('e', 'a', 'x'): out = deref ? regs->eax : (size_t)&regs->eax; break;
		case TextInt('e', 'c', 'x'): out = deref ? regs->ecx : (size_t)&regs->ecx; break;
		case TextInt('e', 'd', 'x'): out = deref ? regs->edx : (size_t)&regs->edx; break;
		case TextInt('e', 'b', 'x'): out = deref ? regs->ebx : (size_t)&regs->ebx; break;
		case TextInt('e', 's', 'p'): out = deref ? regs->esp : (size_t)&regs->esp; break;
		case TextInt('e', 'b', 'p'): out = deref ? regs->ebp : (size_t)&regs->ebp; break;
		case TextInt('e', 's', 'i'): out = deref ? regs->esi : (size_t)&regs->esi; break;
		case TextInt('e', 'd', 'i'): out = deref ? regs->edi : (size_t)&regs->edi; break;
	}
	ret += 3;
	ExpressionLogging("Found register \"%s\"\n", (char*)&cmp);
	return ret;
};

static inline const char* PostfixCheck(const char* expr) {
	if ((expr[0] == '+' && expr[1] == '+') || (expr[0] == '-' && expr[1] == '-')) {
		PostIncDecWarningMessage();
		return expr + 2;
	}
	return expr;
}

static const char* __fastcall consume_value_impl(const char* expr, const char end, size_t *const out, const StackSaver *const data_refs) {
	// Current is used as both "current_char" and "cur_value"
	uint32_t current = 0;
	// cast is used for both casts and pointer sizes
	uint8_t cast = u32_cast;
	const char* expr_next;
	--expr;
	while (1) {
		switch (current = *++expr) {
			// Somehow it ran out of expression string, so stop parsing
			case '\0':
				goto InvalidValueError;
			// Skip whitespace
			case ' ': case '\t': case '\v': case '\f':
				continue;
			// Pointer sizes
			case 'b': case 'B':
				if (strnicmp(expr, "byte ptr", 8) == 0) {
					cast = T8_ptr;
					expr += 7;
					continue;
				}
				goto RawValue;
			case 'w': case 'W':
				if (strnicmp(expr, "word ptr", 8) == 0) {
					cast = T16_ptr;
					expr += 7;
					continue;
				}
				goto InvalidCharacterError;
			case 'd': case 'D':
				if (strnicmp(expr, "dword ptr", 9) == 0) {
					cast = T32_ptr;
					expr += 8;
					continue;
				}
				else if (strnicmp(expr, "double ptr", 10) == 0) {
					cast = T64_ptr;
					expr += 9;
					continue;
				}
				goto RawValue;
			case 'f': case 'F':
				if (strnicmp(expr, "float ptr", 9) == 0) {
					cast = T32_ptr;
					expr += 8;
					continue;
				}
				goto RawValue;
			case 'q': case 'Q':
				if (strnicmp(expr, "qword ptr", 9) == 0) {
					cast = T64_ptr;
					expr += 8;
					continue;
				}
				goto InvalidCharacterError;
			// Unary Operators
			case '!': case '~': case '+': case '-': {
				const uint8_t two_char_unary = expr[1] == current;
				cast = expr[1] == current;
				expr_next = consume_value_impl(expr + 1 + two_char_unary, end, &current, data_refs);
				if (expr + 1 + two_char_unary == expr_next) {
					goto InvalidValueError;
				}
				if (two_char_unary) {
					switch (expr[0]) {
						case '!': current = (bool)current; break;
						//case '~': current = ~~current; break;
						case '-': --current; break;
						case '+': ++current; break;
					}
				}
				else {
					switch (expr[0]) {
						case '!': current = !current; break;
						case '~': current = ~current; break;
						case '-': current *= -1; break;
						//case '+': current = +current; break;
					}
				}
				*out = current;
				return PostfixCheck(expr_next);;
			}
			case '*':
				++expr;
				expr_next = consume_value_impl(expr, end, &current, data_refs);
				if (expr == expr_next) {
					goto InvalidValueError;
				}
				switch (cast) {
					case b_cast: current = (bool)*(uint8_t*)current; break;
					case i8_cast: case u8_cast: current = *(uint8_t*)current; break;
					case i16_cast: case u16_cast: current = *(uint16_t*)current; break;
					case i32_cast: case u32_cast: current = *(uint32_t*)current; break;
					//case i64_cast: case u64_cast: current = (size_t) * (uint64_t*)current; break;
					case f32_cast: current = (size_t) * (float*)current; break;
					//case f64_cast: current = (size_t) * (double*)current; break;
				}
				*out = current;
				return PostfixCheck(expr_next);
			// Casts and subexpression values
			case '(':
				expr_next = CheckCastType(expr + 1, cast);
				if (expr + 1 != expr_next) {
					ExpressionLogging("Cast success\n");
					// Casts
					expr = expr_next;
					expr_next = consume_value_impl(expr, end, &current, data_refs);
					if (expr == expr_next) {
						goto InvalidValueError;
					}
					switch (cast) {
						case i8_cast: current = *(int8_t*)&current; break;
						case i16_cast: current = *(int16_t*)&current; break;
						case i32_cast: current = *(int32_t*)&current; break;
						//case i64_cast: current = *(int64_t*)&current; break;
						case u8_cast: current = *(uint8_t*)&current; break;
						case u16_cast: current = *(uint16_t*)&current; break;
						case u32_cast: current = *(uint32_t*)&current; break;
						//case u64_cast: current = *(uint64_t*)&current; break;
						case f32_cast: current = (uint32_t) * (float*)&current; break;
						// case f64_cast: current = (uint64_t) * (double*)&current; break;
					}
				}
				else {
					// Subexpressions
					// expr + 1 is used to avoid creating a loop
					expr_next = eval_expr_new_impl(expr + 1, ')', &current, StartNoOp, 0, data_refs);
					if (expr + 1 == expr_next) {
						goto InvalidExpressionError;
					}
					if (is_binhack) {
						--expr_next;
					}
				}
				*out = current;
				return PostfixCheck(expr_next);
			// Patch value and/or dereference
			case '[':
				if (is_breakpoint) {
					// Dereference
			case '{':
					// expr + 1 is used to avoid creating a loop
					expr_next = eval_expr_new_impl(expr + 1, current == '[' ? ']' : '}', &current, StartNoOp, 0, data_refs);
					if (expr + 1 == expr_next) {
						goto InvalidExpressionError;
					}
					switch (cast) {
						case b_cast: current = (bool)*(uint8_t*)current; break;
						case i8_cast: case u8_cast: current = *(uint8_t*)current; break;
						case i16_cast: case u16_cast: current = *(uint16_t*)current; break;
						case i32_cast: case u32_cast: current = *(uint32_t*)current; break;
						// case i64_cast: case u64_cast: current = (size_t) * (uint64_t*)current; break;
						case f32_cast: current = (size_t) * (float*)current; break;
						// case f64_cast: current = (size_t) * (double*)current; break;
					}
					*out = current;
					return PostfixCheck(expr_next);
				}
				[[fallthrough]];
			// Guaranteed patch value
			case '<': {
				// DON'T use expr + 1 since that kills get_patch_value
				value_t temp;
				expr_next = get_patch_value_impl(expr, &temp, data_refs);
				if (expr == expr_next) {
					goto InvalidPatchValueError;
				}
				switch (temp.type) {
					case VT_BYTE: current = (uint32_t)temp.b; break;
					case VT_SBYTE: current = (uint32_t)temp.sb; break;
					case VT_WORD: current = (uint32_t)temp.w; break;
					case VT_SWORD: current = (uint32_t)temp.sw; break;
					case VT_DWORD: current = (uint32_t)temp.i; break;
					case VT_SDWORD: current = (uint32_t)temp.si; break;
					case VT_QWORD: current = (uint32_t)temp.q; break;
					case VT_SQWORD: current = (uint32_t)temp.sq; break;
					case VT_FLOAT: current = (uint32_t)temp.f; break;
					case VT_DOUBLE: current = (uint32_t)temp.d; break;
				}
			}
			// If the previous character was the end character,
			// back up so that the main function will be able
			// to detect it and not loop. Yes, it's a bit jank.
			if (expr_next[-1] == end) {
				--expr_next;
			}
			ExpressionLogging("Parsed patch value is %X / %d / %u\n", current, current, current);
			*out = current;
			return PostfixCheck(expr_next);
			// Raw value or breakpoint register
			case '&':
			default:
				expr_next = is_breakpoint ? is_reg_name(expr, data_refs->regs, current) : expr;
				if (expr != expr_next) {
					// current is set inside is_reg_name if a register is detected
					ExpressionLogging("Register value was %X / %d / %u\n", current, current, current);
					*out = current;
					return PostfixCheck(expr_next);
				}
				else {
RawValue:
					// TODO : Don't parse raw hex with the address function
					str_address_ret_t addr_ret;
					current = str_address_value(expr, nullptr, &addr_ret);
					if (expr == addr_ret.endptr || (addr_ret.error && addr_ret.error != STR_ADDRESS_ERROR_GARBAGE)) {
						goto InvalidCharacterError;
					}
					ExpressionLogging(
						"Raw value was %X / %d / %u\n"
						"Remaining after raw value: \"%s\"\n",
						current, current, current, addr_ret.endptr
					);
					*out = current;
					return PostfixCheck(addr_ret.endptr);
				}
		}
	}

InvalidValueError:
	InvalidValueErrorMessage(expr);
	return expr;
InvalidExpressionError:
	ExpressionErrorMessage();
	return expr;
InvalidPatchValueError:
	ValueBracketErrorMessage();
	return expr;
InvalidCharacterError:
	BadCharacterErrorMessage();
	return expr;
}

static __forceinline const char* __fastcall skip_value(const char *const expr, const char end) {
	ExpressionLogging("Skipping until %hhX in \"%s\"...\n", end, expr);
	char temp;
	const char* expr_ref = expr - 1;
	int depth = 0;
	for (;;) {
		switch (temp = *++expr_ref) {
			case '\0':
				if (end == '\0' && depth == 0) {
					return expr_ref;
				}
				return expr;
			default:
				if (temp == end && depth == 0) {
					return expr_ref + 1;
				}
				continue;
			case '(': case '[': case '{':
				++depth;
				continue;
			case ')': case ']': case '}':
				if (temp == end && depth == 0) {
					return expr_ref;
				}
				--depth;
				if (depth < 0) {
					return expr;
				}
				continue;
		}
	}
}

static const char* __fastcall eval_expr_new_impl(const char* expr, const char end, size_t *const out, const op_t start_op, const size_t start_value, const StackSaver *const data_refs) {

	ExpressionLogging(
		"START SUBEXPRESSION: \"%s\"\n"
		"Current value: %X / %d / %u\n"
		"Current end character: \"%hhX\"\n",
		expr, start_value, start_value, start_value, end
	);

	size_t value = start_value;
	struct {
		op_t cur;
		op_t next;
	} ops;
	ops.cur = start_op;
	const char* expr_next;
	size_t cur_value;

	// Yes, this loop is awful looking, but it's intentional
	do {
		ExpressionLogging(
			"Remaining expression: \"%s\"\n"
			"Current character: %hhX\n",
			expr, expr[0]
		);

		expr_next = consume_value_impl(expr, end, &cur_value, data_refs);
		if (expr == expr_next) goto InvalidValueError;
		expr = expr_next;

		expr_next = find_next_op_impl(expr, &ops.next);
		ExpressionLogging(
			"Found operator %hhX: \"%s\"\n"
			"Remaining after op: \"%s\"\n",
			ops.next, PrintOp(ops.next), expr_next
		);

		// Encountering an operator with a higher precedence can
		// be solved by recursing eval_expr over the remaining text
		// and treating the result as a single value
		switch (CompareOpPrecedence(ops.cur, ops.next)) {
			case LowerThanNext:
				ExpressionLogging("\tLOWER PRECEDENCE\n");
				expr = expr_next;
				expr_next = eval_expr_new_impl(expr, end, &cur_value, ops.next, cur_value, data_refs);
				if (expr == expr_next) goto InvalidExpressionError;
				expr = expr_next;
				ExpressionLogging("\tRETURN FROM SUBEXPRESSION\n");
				if (expr[0] == '?') {
					if (OpData.Precedence[ops.cur] < OpData.Precedence[TernaryConditional]) {
						ExpressionLogging("Ternary compare: %s has < precedence\n", PrintOp(ops.cur));
						goto DealWithTernary;
					}
					else {
						ExpressionLogging("Ternary compare: %s has >= precedence\n", PrintOp(ops.cur));
						goto OhCrapItsATernaryHideTheKids;
					}
				}
				else if (expr[0] != end) {
					ExpressionLogging(
						"\tEVIL JANK Remaining: \"%s\"\n"
						"Applying operation: \"%u %s %u\"\n",
						expr, value, PrintOp(ops.cur), cur_value
					);
					value = ApplyOperator(value, cur_value, ops.cur);
					ExpressionLogging("Result of operator was %X / %d / %u\n", value, value, value);
					expr = find_next_op_impl(expr, &ops.cur);
					ExpressionLogging(
						"Found operator %hhX: \"%s\"\n"
						"Remaining after op: \"%s\"\n",
						ops.cur, PrintOp(ops.cur), expr_next
					);
					continue;
				}
				else {
					--expr;
				}
				break;
			case SameAsNext:
				ExpressionLogging("\tSAME PRECEDENCE\n");
				expr = expr_next;
				// This really should be looking at an actual associativity
				// property instead, but everything I tried resulted in
				// VS doubling the allocated stack space per call, which
				// is pretty terrible for a recursive function. Screw that.
				switch (OpData.Precedence[ops.cur]) {
					case OpData.Precedence[NullCoalescing]:
					case OpData.Precedence[TernaryConditional]:
					case OpData.Precedence[Assign]:
						expr_next = eval_expr_new_impl(expr, end, &cur_value, ops.next, cur_value, data_refs);
						if (expr == expr_next) goto InvalidExpressionError;
						expr = expr_next - 1;
				}
				/*if (OpData.Associativity[ops.cur] == RightAssociative) {
					expr_next = eval_expr_new_impl(expr, end, &cur_value, ops.next, cur_value, data_refs);
					if (expr == expr_next) goto InvalidExpressionError;
					expr = expr_next - 1;
				}*/
				break;
			//case HigherThanNext:
			default:
				if (OpData.Precedence[ops.next] != OpData.Precedence[NoOp] && start_op != StartNoOp) {
					ExpressionLogging("\tHIGHER PRECEDENCE\n");
					// Higher precedence quick exit
					ExpressionLogging("Applying operation: \"%u %s %u\"\n", value, PrintOp(ops.cur), cur_value);
					*out = ApplyOperator(value, cur_value, ops.cur);
					ExpressionLogging("Result of operator was %X / %d / %u\n", *out, *out, *out);
					return expr + 1;
				}
				expr = expr_next;
		}

		ExpressionLogging("Applying operation: \"%u %s %u\"\n", value, PrintOp(ops.cur), cur_value);
		value = ApplyOperator(value, cur_value, ops.cur);
		ExpressionLogging("Result of operator was %X / %d / %u\n", value, value, value);

		// This block can probably be handled better
		if (ops.next == TernaryConditional || ops.next == Elvis) {
			if (start_op == StartNoOp) {
				ExpressionLogging("Ternary with no preceding ops\n");
				goto DealWithTernary;
			}
			else {
				goto OhCrapItsATernaryHideTheKids;
			}
		}
		else {
			ops.cur = ops.next;
		}

	} while (expr[0] != end);
	ExpressionLogging(
		"END SUBEXPRESSION \"%s\"\n"
		"Subexpression value was %X / %d / %u\n",
		expr, value, value, value
	);
	*out = value;
	return expr + 1;

// TODO: Find a way of integrating this better...
// For whatever reason, everything I've tried to
// get rid of this goto has ended up doubling the
// stack usage of this function. Considering that
// it's a recursive function and stack space is
// valuable, the goto seems to be worth it.
DealWithTernary:
	if (cur_value) {
		if (expr++[1] != ':') {
			expr_next = eval_expr_new_impl(expr, ':', &cur_value, StartNoOp, 0, data_refs);
			if (expr == expr_next) goto InvalidExpressionError;
			expr = expr_next;
		}
		expr_next = skip_value(expr, end);
		if (expr == expr_next) goto InvalidValueError;
		expr = expr_next;
		ExpressionLogging("Ternary TRUE remaining: \"%s\"\n", expr);
	}
	else {
		expr_next = skip_value(expr, ':');
		if (expr == expr_next) goto InvalidValueError;
		expr = expr_next;
		expr_next = eval_expr_new_impl(expr, end, &cur_value, StartNoOp, 0, data_refs);
		if (expr == expr_next) goto InvalidExpressionError;
		expr = expr_next - 1;
		ExpressionLogging("Ternary FALSE remaining: \"%s\"\n", expr);
	}
	ExpressionLogging(
		"Remaining after ternary: \"%s\"\n"
		"Applying operation: \"%u %s %u\"\n",
		expr, value, PrintOp(ops.cur), cur_value
	);
	if (expr[0] == end) {
		expr += end != '\0';
		*out = ApplyOperator(value, cur_value, ops.cur);
	}
	else {
		expr = find_next_op_impl(expr, &ops.cur);
		expr_next = eval_expr_new_impl(expr, end, out, ops.cur, 0, data_refs);
		if (expr == expr_next) goto InvalidExpressionError;
		expr = expr_next;
	}
	ExpressionLogging(
		"Result of operator was %X / %d / %u\n"
		"END TERNARY SUBEXPRESSION \"%s\"\n"
		"Subexpression value was %X / %d / %u\n",
		*out, *out, *out, expr, *out, *out, *out
	);
	return expr + 1;

OhCrapItsATernaryHideTheKids:
	ExpressionLogging(
		"END SUBEXPRESSION FOR TERNARY \"%s\"\n"
		"Subexpression value was %X / %d / %u\n",
		expr, value, value, value
	);
	*out = value;
	return expr;

InvalidValueError:
	InvalidValueErrorMessage(expr);
	return expr;
InvalidExpressionError:
	ExpressionErrorMessage();
	return expr;
}

const char* __fastcall eval_expr(const char* expr, char end, size_t* out, x86_reg_t* regs, size_t rel_source) {
	ExpressionLogging("START EXPRESSION \"%s\" with end \"%hhX\"\n", expr, end);

	const StackSaver data_refs = { regs, rel_source };

	const char *const expr_next = eval_expr_new_impl(expr, end, out, StartNoOp, 0, &data_refs);
	if (expr == expr_next) goto ExpressionError;
	ExpressionLogging(
		"END EXPRESSION\n"
		"\t\t\t\t\t\t\t\t\t\t\t\t\tExpression was: \"%s\"\n"
		"\t\t\t\t\t\t\t\t\t\t\t\t\tFINAL result was: %X / %d / %u\n"
		"Remaining after final: \"%s\"\n",
		expr, *out, *out, *out, expr_next
	);
	return expr_next;

ExpressionError:
	ExpressionErrorMessage();
	*out = 0;
	ExpressionLogging(
		"END EXPRESSION WITH ERROR\n"
		"\t\t\t\t\t\t\t\t\t\t\t\t\tExpression was: \"%s\"\n"
		"\t\t\t\t\t\t\t\t\t\t\t\t\tFINAL result was: %X / %d / %u\n"
		"Remaining after final: \"%s\"\n",
		expr, *out, *out, *out, expr_next
	);
	return expr_next;
}
