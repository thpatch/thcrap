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
// Using __noop makes the compiler check the validity of
// the macro contents without actually compiling them.
#define ExpressionLogging(...) __noop(__VA_ARGS__)
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
		log_printf("EXPRESSION WARNING 3: Codecave \"%s\" not found! Returning NULL...\n", name);
	}
}

static __declspec(noinline) void PostIncDecWarningMessage(void) {
	WarnOnce(log_printf("EXPRESSION WARNING 4: Postfix increment and decrement operators do not currently function as expected because it is not possible to modify the value of an option in an expression. These operators do nothing and are only included for future compatibility and operator precedence reasons.\n"));
}

static __declspec(noinline) void InvalidCPUFeatureWarningMessage(const char *const name) {
	log_printf("EXPRESSION WARNING 5: Unknown CPU feature \"%s\"! Assuming feature is present and returning 1...\n");
}

//static __declspec(noinline) void InvalidCodeOptionWarningMessage(void) {
//	log_printf("EXPRESSION WARNING 6: Code options are not valid in expressions! Returning NULL...\n");
//}

static __declspec(noinline) void NullDerefWarningMessage(void) {
	log_printf("EXPRESSION WARNING 7: Attempted to dereference NULL value! Returning NULL...\n");
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
static const char* __fastcall consume_value_impl(const char* expr, size_t *const out, const StackSaver *const data_refs);

size_t* reg(x86_reg_t *regs, const char *regname, const char **endptr) {
	// Verify pointers and regname is at least 4 bytes
	if (!regs || !regname || !regname[0] || !regname[1] || !regname[2]) {
		return NULL;
	}
	// Just make sure that endptr can be written to unconditionally
	if (!endptr) endptr = (const char**)&endptr;

	*endptr = regname + 3;
	// Since regname is guaranteed to be at least 4
	// bytes long and none of `{|}~ are a valid register
	// name, the whole string can be converted to a
	// single case by clearing bit 5 of each character.
	switch (*(uint32_t*)regname & TextInt(0xDF, 0xDF, 0xDF, '\0')) {
		case TextInt('E', 'A', 'X'): return &regs->eax;
		case TextInt('E', 'C', 'X'): return &regs->ecx;
		case TextInt('E', 'D', 'X'): return &regs->edx;
		case TextInt('E', 'B', 'X'): return &regs->ebx;
		case TextInt('E', 'S', 'P'): return &regs->esp;
		case TextInt('E', 'B', 'P'): return &regs->ebp;
		case TextInt('E', 'S', 'I'): return &regs->esi;
		case TextInt('E', 'D', 'I'): return &regs->edi;
		default:
			*endptr = regname;
			return NULL;
	}
}

#define PreventOverlap(val) ((val) + (val))

#pragma warning(push)
// Intentional overflow/wraparound on some values,
// particularly variants of "or". Value only needs
// to be unique and easily calculated.
#pragma warning(disable : 4309 4369)
enum : uint8_t {
	NullOp = 0,
	StartNoOp = 1,
	EndGroupOp = 2,
	StandaloneTernaryEnd = ':',
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
	CircularLeftShift = PreventOverlap('R') + '<' + '<',
	CircularRightShift = PreventOverlap('R') + '>' + '>',
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
	LogicalAndSC = LogicalAnd - 1,
	LogicalNand = PreventOverlap('!') + PreventOverlap('&') + PreventOverlap('&'),
	LogicalNandSC = LogicalNand - 1,
	LogicalXor = '^' + '^',
	LogicalXnor = PreventOverlap('!') + '^' + '^',
	LogicalOr = PreventOverlap('|') + PreventOverlap('|'),
	LogicalOrSC = LogicalOr - 1,
	LogicalNor = PreventOverlap('!') + PreventOverlap('|') + PreventOverlap('|'),
	LogicalNorSC = LogicalNor - 1,
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
	CircularLeftShiftAssign = PreventOverlap('R') + '<' + '<' + '=',
	CircularRightShiftAssign = PreventOverlap('R') + '>' + '>' + '=',
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
#define ERROR_PRECEDENCE UINT8_MAX
#define ERROR_ASSOCIATIVITY LeftAssociative
		Precedence[BadBrackets] = ERROR_PRECEDENCE;
		Associativity[BadBrackets] = ERROR_ASSOCIATIVITY;
#define POWER_PRECEDENCE 21
#define POWER_ASSOCIATIVITY LeftAssociative
		Precedence[Power] = POWER_PRECEDENCE;
		Associativity[Power] = POWER_ASSOCIATIVITY;
#define MULTIPLY_PRECEDENCE 19
#define MULTIPLY_ASSOCIATIVITY LeftAssociative
		Precedence[Multiply] = Precedence[Divide] = Precedence[Modulo] = MULTIPLY_PRECEDENCE;
		Associativity[Multiply] = Associativity[Divide] = Associativity[Modulo] = MULTIPLY_ASSOCIATIVITY;
#define ADD_PRECEDENCE 18
#define ADD_ASSOCIATIVITY LeftAssociative
		Precedence[Add] = Precedence[Subtract] = ADD_PRECEDENCE;
		Associativity[Add] = Associativity[Subtract] = ADD_ASSOCIATIVITY;
#define SHIFT_PRECEDENCE 17
#define SHIFT_ASSOCIATIVITY LeftAssociative
		Precedence[LogicalLeftShift] = Precedence[LogicalRightShift] = Precedence[ArithmeticLeftShift] = Precedence[ArithmeticRightShift] = Precedence[CircularLeftShift] = Precedence[CircularRightShift] = SHIFT_PRECEDENCE;
		Associativity[LogicalLeftShift] = Associativity[LogicalRightShift] = Associativity[ArithmeticLeftShift] = Associativity[ArithmeticRightShift] = Associativity[CircularLeftShift] = Associativity[CircularRightShift] = SHIFT_ASSOCIATIVITY;
#define COMPARE_PRECEDENCE 16
#define COMPARE_ASSOCIATIVITY LeftAssociative
		Precedence[Less] = Precedence[LessEqual] = Precedence[Greater] = Precedence[GreaterEqual] = COMPARE_PRECEDENCE;
		Associativity[Less] = Associativity[LessEqual] = Associativity[Greater] = Associativity[GreaterEqual] = COMPARE_ASSOCIATIVITY;
#define EQUALITY_PRECEDENCE 15
#define EQUALITY_ASSOCIATIVITY LeftAssociative
		Precedence[Equal] = Precedence[NotEqual] = EQUALITY_PRECEDENCE;
		Associativity[Equal] = Associativity[NotEqual] = EQUALITY_ASSOCIATIVITY;
#define THREEWAY_PRECEDENCE 14
#define THREEWAY_ASSOCIATIVITY LeftAssociative
		Precedence[ThreeWay] = THREEWAY_PRECEDENCE;
		Associativity[ThreeWay] = THREEWAY_ASSOCIATIVITY;
#define BITAND_PRECEDENCE 13
#define BITAND_ASSOCIATIVITY LeftAssociative
		Precedence[BitwiseAnd] = Precedence[BitwiseNand] = BITAND_PRECEDENCE;
		Associativity[BitwiseAnd] = Associativity[BitwiseNand] = BITAND_ASSOCIATIVITY;
#define BITXOR_PRECEDENCE 12
#define BITXOR_ASSOCIATIVITY LeftAssociative
		Precedence[BitwiseXor] = Precedence[BitwiseXnor] = BITXOR_PRECEDENCE;
		Associativity[BitwiseXor] = Associativity[BitwiseXnor] = BITXOR_ASSOCIATIVITY;
#define BITOR_PRECEDENCE 11
#define BITOR_ASSOCIATIVITY LeftAssociative
		Precedence[BitwiseOr] = Precedence[BitwiseNor] = BITOR_PRECEDENCE;
		Associativity[BitwiseOr] = Associativity[BitwiseNor] = BITOR_ASSOCIATIVITY;
#define AND_PRECEDENCE 10
#define AND_ASSOCIATIVITY LeftAssociative
		Precedence[LogicalAnd] = Precedence[LogicalNand] = Precedence[LogicalAndSC] = Precedence[LogicalNandSC] = AND_PRECEDENCE;
		Associativity[LogicalAnd] = Associativity[LogicalNand] = Associativity[LogicalAndSC] = Associativity[LogicalNandSC] = AND_ASSOCIATIVITY;
#define XOR_PRECEDENCE 9
#define XOR_ASSOCIATIVITY LeftAssociative
		Precedence[LogicalXor] = Precedence[LogicalXnor] = XOR_PRECEDENCE;
		Associativity[LogicalXor] = Associativity[LogicalXnor] = XOR_ASSOCIATIVITY;
#define OR_PRECEDENCE 8
#define OR_ASSOCIATIVITY LeftAssociative
		Precedence[LogicalOr] = Precedence[LogicalNor] = Precedence[LogicalOrSC] = Precedence[LogicalNorSC] = OR_PRECEDENCE;
		Associativity[LogicalOr] = Associativity[LogicalNor] = Associativity[LogicalOrSC] = Associativity[LogicalNorSC] = OR_ASSOCIATIVITY;
#define NULLC_PRECEDENCE 7
#define NULLC_ASSOCIATIVITY RightAssociative
		Precedence[NullCoalescing] = NULLC_PRECEDENCE;
		Associativity[NullCoalescing] = NULLC_ASSOCIATIVITY;
#define TERNARY_PRECEDENCE 6
#define TERNARY_ASSOCIATIVITY RightAssociative
		Precedence[TernaryConditional] = Precedence[Elvis] = TERNARY_PRECEDENCE;
		Associativity[TernaryConditional] = Associativity[Elvis] = TERNARY_ASSOCIATIVITY;
#define ASSIGN_PRECEDENCE 5
#define ASSIGN_ASSOCIATIVITY RightAssociative
		Precedence[Assign] = Precedence[AddAssign] = Precedence[SubtractAssign] = Precedence[MultiplyAssign] = Precedence[DivideAssign] = Precedence[ModuloAssign] = Precedence[LogicalLeftShiftAssign] = Precedence[LogicalRightShiftAssign] = Precedence[ArithmeticLeftShiftAssign] = Precedence[ArithmeticRightShiftAssign] = Precedence[CircularLeftShiftAssign] = Precedence[CircularRightShiftAssign] = Precedence[AndAssign] = Precedence[OrAssign] = Precedence[XorAssign] = Precedence[NandAssign] = Precedence[XnorAssign] = Precedence[NorAssign] = Precedence[NullCoalescingAssign] = ASSIGN_PRECEDENCE;
		Associativity[Assign] = Associativity[AddAssign] = Associativity[SubtractAssign] = Associativity[MultiplyAssign] = Associativity[DivideAssign] = Associativity[ModuloAssign] = Associativity[LogicalLeftShiftAssign] = Associativity[LogicalRightShiftAssign] = Associativity[ArithmeticLeftShiftAssign] = Associativity[ArithmeticRightShiftAssign] = Associativity[CircularLeftShiftAssign] = Associativity[CircularRightShiftAssign] = Associativity[AndAssign] = Associativity[OrAssign] = Associativity[XorAssign] = Associativity[NandAssign] = Associativity[XnorAssign] = Associativity[NorAssign] = Associativity[NullCoalescingAssign] = ASSIGN_ASSOCIATIVITY;
#define COMMA_PRECEDENCE 4
#define COMMA_ASSOCIATIVITY LeftAssociative
		Precedence[Comma] = COMMA_PRECEDENCE;
		Associativity[Comma] = COMMA_ASSOCIATIVITY;
#define GOMMA_PRECEDENCE 3
#define GOMMA_ASSOCIATIVITY LeftAssociative
		Precedence[Gomma] = GOMMA_PRECEDENCE;
		Associativity[Gomma] = GOMMA_ASSOCIATIVITY;
#define STARTOP_PRECEDENCE 2
#define STARTOP_ASSOCIATIVITY LeftAssociative
		Precedence[StartNoOp] = Precedence[StandaloneTernaryEnd] = STARTOP_PRECEDENCE;
		Associativity[StartNoOp] = Precedence[StandaloneTernaryEnd] = STARTOP_ASSOCIATIVITY;
#define END_GROUP_NOOP_PRECEDENCE 0
#define END_GROUP_NOOP_ASSOCIATIVITY LeftAssociative
		Precedence[EndGroupOp] = END_GROUP_NOOP_PRECEDENCE;
		Associativity[EndGroupOp] = END_GROUP_NOOP_ASSOCIATIVITY;
#define NOOP_PRECEDENCE 0
#define NOOP_ASSOCIATIVITY LeftAssociative
		Precedence[NullOp] = NOOP_PRECEDENCE;
		Associativity[NullOp] = NOOP_ASSOCIATIVITY;
	}
};

static constexpr OpData_t OpData;

static inline const char* find_matching_end(const char* str, const uint32_t delims_in) {
	if (!str) return NULL;
	const union {
		const uint32_t in;
		struct {
			const char start;
			const char end;
		};
	} delims = { delims_in };
	size_t depth = 0;
	for (char c; c = *str; ++str) {
		depth += (c == delims.start) - (c == delims.end);
		if (!depth) return str;
	}
	return NULL;
};

const char* parse_brackets(const char* str, char c) {
	--str;
	int32_t paren_count = 0;
	int32_t square_count = 0;
	int32_t curly_count = 0;
	do {
		paren_count += (c == '(') - (c == ')');
		square_count += (c == '[') - (c == ']');
		curly_count += (c == '{') - (c == '}');
		if (const int32_t temp = (paren_count | square_count | curly_count);
			temp == 0) {
			return str;
		}
		else if (temp < 0) {
			return NULL;
		}
	} while (c = *++str);
	return NULL;
}

static __forceinline const char* __fastcall find_next_op_impl(const char *const expr, op_t *const out) {
	uint8_t c;
	const char* expr_ref = expr - 1;
	while (1) {
		switch (c = *++expr_ref) {
			case '\0':
				*out = c;
				return expr;
			case '(': case '[': case '{':
				*out = BadBrackets;
				return expr;
			case ')': case ']': case '}':
				*out = EndGroupOp;
				return expr_ref;
			case '~':
				switch (uint8_t temp = expr_ref[1]) {
					case '&': case '|':
						temp += temp;
						[[fallthrough]];
					case '^':
						switch (expr_ref[2]) {
							case '=':
								*out = c + temp + '=';
								return expr_ref + 3;
							default:
								*out = c + temp;
								return expr_ref + 2;
						}
				}
				continue;
			case '!':
				c += c;
				if (uint8_t temp = expr_ref[1]; temp == '=') {
					goto CPlusEqualRetPlus2;
				}
				else {
					switch (temp) {
						case '&': case '|':
							temp += temp;
							[[fallthrough]];
						case '^':
							if (expr_ref[2] == expr_ref[1]) {
								*out = c + temp * 2;
								return expr_ref + 3;
							}
					}
				}
				continue;
			case '?':
				c += c;
				switch (expr_ref[1]) {
					case '?':
						switch (expr_ref[2]) {
							case '=':	goto CTimes2PlusEqualRetPlus3;
							default:	goto CTimes2RetPlus2;
						}
					default:
						// All of these offset expr_ref by
						// 1 less than normal in order to
						// more easily process the operators.
						*out = c;
						return expr_ref;
				}
			case '<': case '>':
				if (expr_ref[1]) {
					uint32_t temp;

					if (expr_ref[2]) {
						switch (temp = *(uint32_t*)expr_ref & TextInt(0xFD, 0xFD, 0xFD, 0xFF)) {
							case TextInt('<', '<', '<', '='):
								*out = c * 3 + '=';
								return expr_ref + 4;
							default:
								switch (temp &= TextInt(0xFF, 0xFF, 0xFF, '\0')) {
									case TextInt('<', '=', '<'):
										*out = ThreeWay;
										return expr_ref + 3;
									case TextInt('<', '<', '='):
										goto CTimes2PlusEqualRetPlus3;
									case TextInt('<', '<', '<'):
										*out = c * 3;
										return expr_ref + 3;
								}
						}
						temp &= TextInt(0xFF, 0xFF, '\0');
					}
					else {
						temp = *(uint16_t*)expr_ref & TextInt(0xFD, 0xFD, '\0');
					}
					switch (temp) {
						case TextInt('<', '='):	goto CPlusEqualRetPlus2;
						case TextInt('<', '<'):	goto CTimes2RetPlus2;
					}
				}
				goto CRetPlus1;

			case 'r': case 'R':
				if (expr_ref[1] && expr_ref[2]) {
					c |= 0x20;
					c += c;
					switch (uint32_t temp = *(uint32_t*)expr_ref | TextInt(0x20, 0x00, 0x00, 0x00)) {
						case TextInt('r', '>', '>', '='):
							*out = c + '>' + '>' + '=';
							return expr_ref + 4;
						case TextInt('r', '<', '<', '='):
							*out = c + '<' + '<' + '=';
							return expr_ref + 4;
						default:
							switch (temp & TextInt(0xFF, 0xFF, 0xFF, '\0')) {
								case TextInt('r', '>', '>'):
									*out = c + '>' + '>';
									return expr_ref + 3;
								case TextInt('r', '<', '<'):
									*out = c + '<' + '<';
									return expr_ref + 3;
							}
					}
				}
				continue;
			case '&':
				c += c;
				switch (expr_ref[1]) {
					case '&':	goto CTimes2RetPlus2;
					case '=':	goto CPlusEqualRetPlus2;
					default:	goto CRetPlus1;
				}
			case '|':
				c += c;
				switch (expr_ref[1]) {
					case '|':	goto CTimes2RetPlus2;
					case '=':	goto CPlusEqualRetPlus2;
					default:	goto CRetPlus1;
				}
			case '*':
				switch (expr_ref[1]) {
					case '*':	goto CTimes2RetPlus2;
					case '=':	goto CPlusEqualRetPlus2;
					default:	goto CRetPlus1;
				}
			case '^':
				switch (expr_ref[1]) {
					case '^':	goto CTimes2RetPlus2;
					case '=':	goto CPlusEqualRetPlus2;
					default:	goto CRetPlus1;
				}
			case '+': case '-': case '/': case '%': case '=':
				switch (expr_ref[1]) {
					case '=':	goto CPlusEqualRetPlus2;
					default:	goto CRetPlus1;
				}
			case ',': case ';': case ':':
				goto CRetPlus1;
		}
	}

CRetPlus1:
	*out = c;
	return expr_ref + 1;

CTimes2RetPlus2:
	*out = c * 2;
	return expr_ref + 2;

CPlusEqualRetPlus2:
	*out = c + '=';
	return expr_ref + 2;

CTimes2PlusEqualRetPlus3:
	*out = c * 2 + '=';
	return expr_ref + 3;
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
		case CircularLeftShift: return "R<<";
		case CircularRightShift: return "R>>";
		case BitwiseAnd: return "&";
		case BitwiseNand: return "~&";
		case BitwiseXor: return "^";
		case BitwiseXnor: return "~^";
		case BitwiseOr: return "|";
		case BitwiseNor: return "~|";
		case LogicalAnd: return "&&";
		case LogicalAndSC: return "&&SC";
		case LogicalNand: return "!&&";
		case LogicalNandSC: return "!&&SC";
		case LogicalXor: return "^^";
		case LogicalXnor: return "!^^";
		case LogicalOr: return "||";
		case LogicalOrSC: return "||SC";
		case LogicalNor: return "!||";
		case LogicalNorSC: return "!||SC";
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
		case CircularLeftShiftAssign: return "R<<=";
		case CircularRightShiftAssign: return "R>>=";
		case AndAssign: return "&=";
		case XorAssign: return "^=";
		case OrAssign: return "|=";
		case NandAssign: return "~&=";
		case XnorAssign: return "~^=";
		case NorAssign: return "~|=";
		case NullCoalescingAssign: return "??=";
		case Comma: return ",";
		case Gomma: return ";";
		case NullOp: return "NullOp";
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

static inline size_t __fastcall ApplyPower(size_t value, size_t arg) {
	if (arg == 0) return 1;
	size_t result = 1;
	switch (unsigned long power; _BitScanReverse(&power, arg), power) {
		case 4:
			result *= arg & 1 ? value : 1;
			arg >>= 1;
			value *= value;
		case 3:
			result *= arg & 1 ? value : 1;
			arg >>= 1;
			value *= value;
		case 2:
			result *= arg & 1 ? value : 1;
			arg >>= 1;
			value *= value;
		case 1:
			result *= arg & 1 ? value : 1;
			arg >>= 1;
			value *= value;
		case 0:
			result *= arg & 1 ? value : 1;
			return result;
		default:
			return UINT_MAX;
	}
}

static size_t ApplyOperator(const size_t value, const size_t arg, const op_t op) {
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
			AssignmentWarningMessage();
			[[fallthrough]];
		case CircularLeftShift: // Compiles to ROL
			return value << arg | value >> (sizeof(value) * CHAR_BIT - arg);
		case CircularRightShiftAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case CircularRightShift: // Compiles to ROR
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
			return (value > arg) - (value < arg);
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
		case LogicalAnd: case LogicalAndSC:
			return value && arg;
		case LogicalNand: case LogicalNandSC:
			return !(value && arg);
		case LogicalXor:
			return (bool)(value ^ arg);
		case LogicalXnor:
			return (bool)!(value ^ arg);
		case LogicalOr: case LogicalOrSC:
			return value || arg;
		case LogicalNor: case LogicalNorSC:
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
		case NullOp:
		case EndGroupOp:
		case StandaloneTernaryEnd:
		default:
			return arg;
	}
}

static __declspec(noinline) const patch_val_t* GetOptionValue(const char *const name, const size_t name_length) {
	const char *const name_buffer = strndup(name, name_length);
	ExpressionLogging("Option: \"%s\"\n", name_buffer);
	const patch_val_t *const option = patch_opt_get(name_buffer);
	if (!option) {
		OptionNotFoundErrorMessage(name_buffer);
	}
	free((void*)name_buffer);
	return option;
}

static __declspec(noinline) const patch_val_t* GetPatchTestValue(const char *const name, const size_t name_length) {
	const char *const name_buffer = strndup(name, name_length);
	ExpressionLogging("PatchTest: \"%s\"\n", name_buffer);
	const patch_val_t *const patch_test = patch_opt_get(name_buffer);
	free((void*)name_buffer);
	return patch_test;
}

static __declspec(noinline) bool GetCPUFeatureTest(const char *const name, const size_t name_length) {
	const char *const name_buffer = strndup(name, name_length);
	ExpressionLogging("CPUFeatureTest: \"%s\"\n", name_buffer);
	bool ret = false;
	// Yuck
	switch (name_length) {
		case 15:
			if		(stricmp(name_buffer, "avx512vpopcntdq") == 0) ret = CPUID_Data.HasAVX512VPOPCNTDQ;
			else	goto InvalidCPUFeatureError;
			break;
		case 12:
			if		(stricmp(name_buffer, "avx512bitalg") == 0) ret = CPUID_Data.HasAVX512BITALG;
			else if (stricmp(name_buffer, "avx5124fmaps") == 0) ret = CPUID_Data.HasAVX5124FMAPS;
			else if (stricmp(name_buffer, "avx5124vnniw") == 0) ret = CPUID_Data.HasAVX5124VNNIW;
			else	goto InvalidCPUFeatureError;
			break;
		case 11:
			if		(stricmp(name_buffer, "avx512vbmi1") == 0) ret = CPUID_Data.HasAVX512VBMI;
			else if (stricmp(name_buffer, "avx512vbmi2") == 0) ret = CPUID_Data.HasAVX512VBMI2;
			else	goto InvalidCPUFeatureError;
			break;
		case 10:
			if		(stricmp(name_buffer, "cmpxchg16b") == 0) ret = CPUID_Data.HasCMPXCHG16B;
			else if (stricmp(name_buffer, "vpclmulqdq") == 0) ret = CPUID_Data.HasVPCLMULQDQ;
			else if (stricmp(name_buffer, "avx512ifma") == 0) ret = CPUID_Data.HasAVX512IFMA;
			else if (stricmp(name_buffer, "avx512vnni") == 0) ret = CPUID_Data.HasAVX512VNNI;
			else if (stricmp(name_buffer, "avx512vp2i") == 0) ret = CPUID_Data.HasAVX512VP2I;
			else if (stricmp(name_buffer, "avx512bf16") == 0) ret = CPUID_Data.HasAVX512BF16;
			else	goto InvalidCPUFeatureError;
			break;
		case 9:
			if		(stricmp(name_buffer, "pclmulqdq") == 0) ret = CPUID_Data.HasPCLMULQDQ;
			else	goto InvalidCPUFeatureError;
			break;
		case 8:
			if		(stricmp(name_buffer, "cmpxchg8") == 0) ret = CPUID_Data.HasCMPXCHG8;
			else if (stricmp(name_buffer, "avx512dq") == 0) ret = CPUID_Data.HasAVX512DQ;
			else if (stricmp(name_buffer, "avx512pf") == 0) ret = CPUID_Data.HasAVX512PF;
			else if (stricmp(name_buffer, "avx512er") == 0) ret = CPUID_Data.HasAVX512ER;
			else if (stricmp(name_buffer, "avx512cd") == 0) ret = CPUID_Data.HasAVX512CD;
			else if (stricmp(name_buffer, "avx512bw") == 0) ret = CPUID_Data.HasAVX512BW;
			else if (stricmp(name_buffer, "avx512vl") == 0) ret = CPUID_Data.HasAVX512VL;
			else if (stricmp(name_buffer, "3dnowext") == 0) ret = CPUID_Data.Has3DNOWEXT;
			else	goto InvalidCPUFeatureError;
			break;
		case 7:
			if		(stricmp(name_buffer, "avx512f") == 0) ret = CPUID_Data.HasAVX512F;
			else	goto InvalidCPUFeatureError;
			break;
		case 6:
			if		(stricmp(name_buffer, "popcnt") == 0) ret = CPUID_Data.HasPOPCNT;
			else if (stricmp(name_buffer, "fxsave") == 0) ret = CPUID_Data.HasFXSAVE;
			else if (stricmp(name_buffer, "mmxext") == 0) ret = CPUID_Data.HasMMXEXT;
			else	goto InvalidCPUFeatureError;
			break;
		case 5:
			if		(stricmp(name_buffer, "intel") == 0) ret = CPUID_Data.Manufacturer == Intel;
			else if (stricmp(name_buffer, "ssse3") == 0) ret = CPUID_Data.HasSSSE3;
			else if (stricmp(name_buffer, "sse41") == 0) ret = CPUID_Data.HasSSE41;
			else if (stricmp(name_buffer, "sse42") == 0) ret = CPUID_Data.HasSSE42;
			else if (stricmp(name_buffer, "sse4a") == 0) ret = CPUID_Data.HasSSE4A;
			else if (stricmp(name_buffer, "movbe") == 0) ret = CPUID_Data.HasMOVBE;
			else if (stricmp(name_buffer, "3dnow") == 0) ret = CPUID_Data.Has3DNOW;
			else	goto InvalidCPUFeatureError;
			break;
		case 4:
			if		(stricmp(name_buffer, "cmov") == 0) ret = CPUID_Data.HasCMOV;
			else if (stricmp(name_buffer, "sse2") == 0) ret = CPUID_Data.HasSSE2;
			else if (stricmp(name_buffer, "sse3") == 0) ret = CPUID_Data.HasSSE3;
			else if (stricmp(name_buffer, "avx2") == 0) ret = CPUID_Data.HasAVX2;
			else if (stricmp(name_buffer, "bmi1") == 0) ret = CPUID_Data.HasBMI1;
			else if (stricmp(name_buffer, "bmi2") == 0) ret = CPUID_Data.HasBMI2;
			else if (stricmp(name_buffer, "erms") == 0) ret = CPUID_Data.HasERMS;
			else if (stricmp(name_buffer, "fsrm") == 0) ret = CPUID_Data.HasFSRM;
			else if (stricmp(name_buffer, "f16c") == 0) ret = CPUID_Data.HasF16C;
			else if (stricmp(name_buffer, "gfni") == 0) ret = CPUID_Data.HasGFNI;
			else if (stricmp(name_buffer, "fma4") == 0) ret = CPUID_Data.HasFMA4;
			else	goto InvalidCPUFeatureError;
			break;
		case 3:
			if		(stricmp(name_buffer, "amd") == 0) ret = CPUID_Data.Manufacturer == AMD;
			else if (stricmp(name_buffer, "sse") == 0) ret = CPUID_Data.HasSSE;
			else if (stricmp(name_buffer, "fma") == 0) ret = CPUID_Data.HasFMA;
			else if (stricmp(name_buffer, "mmx") == 0) ret = CPUID_Data.HasMMX;
			else if (stricmp(name_buffer, "avx") == 0) ret = CPUID_Data.HasAVX;
			else if (stricmp(name_buffer, "adx") == 0) ret = CPUID_Data.HasADX;
			else if (stricmp(name_buffer, "abm") == 0) ret = CPUID_Data.HasABM;
			else if (stricmp(name_buffer, "xop") == 0) ret = CPUID_Data.HasXOP;
			else if (stricmp(name_buffer, "tbm") == 0) ret = CPUID_Data.HasTBM;
			else	goto InvalidCPUFeatureError;
			break;
		default:
InvalidCPUFeatureError:
			InvalidCPUFeatureWarningMessage(name_buffer);
	}
	free((void*)name_buffer);
	return ret;
}

static __declspec(noinline) size_t GetCodecaveAddress(const char *const name, const size_t name_length, const bool is_relative, const StackSaver *const data_refs) {
	char *const name_buffer = strndup(name, name_length);
	ExpressionLogging("CodecaveAddress: \"%s\"\n", name_buffer);
	char* user_offset_expr = strchr(name_buffer, '+');
	if (user_offset_expr) {
		// This prevents next read to name_buffer
		// from containing the offset value
		*user_offset_expr++ = '\0';
	}
	size_t cave_addr = func_get(name_buffer);
	if (!cave_addr) {
		CodecaveNotFoundWarningMessage(name_buffer);
	}
	else {
		if (user_offset_expr) {
			char* user_offset_expr_next;
			// Try a hex value by default for compatibility
			size_t user_offset_value = strtoul(user_offset_expr, &user_offset_expr_next, 16);
			switch ((bool)(user_offset_expr == user_offset_expr_next)) {
				case true:
					// If a hex value doesn't work, try a subexpression
					if (!eval_expr_new_impl(user_offset_expr, '\0', &user_offset_value, StartNoOp, 0, data_refs)) {
						ExpressionErrorMessage();
						break;
					}
					[[fallthrough]];
				default:
					cave_addr += user_offset_value;
			}
		}
		if (is_relative) {
			cave_addr -= data_refs->rel_source + sizeof(void*);
		}
	}
	free(name_buffer);
	return cave_addr;
}

static __declspec(noinline) size_t GetBPFuncOrRawAddress(const char *const name, const size_t name_length, const bool is_relative, const StackSaver *const data_refs) {
	const char *const name_buffer = strndup(name, name_length);
	ExpressionLogging("BPFuncOrRawAddress: \"%s\"\n", name_buffer);
	size_t addr = func_get(name_buffer);
	switch (addr) {
		case 0: // Will be null if the name was not a BP function
			if (!eval_expr_new_impl(name_buffer, '\0', &addr, StartNoOp, 0, data_refs)) {
				ExpressionErrorMessage();
				// addr would still be 0, so break to
				// free the name string and return 0
				break;
			}
			[[fallthrough]];
		default:
			if (is_relative) {
				addr -= data_refs->rel_source + sizeof(void*);
			}
	}
	free((void*)name_buffer);
	return addr;
}

static __declspec(noinline) const char* get_patch_value_impl(const char* expr, patch_val_t *const out, const StackSaver *const data_refs) {

	ExpressionLogging("Patch value opening char: \"%hhX\"\n", expr[0]);
	const bool is_relative = expr[0] == '[';
	const char *const patch_val_end = find_matching_end(expr, is_relative ? TextInt('[', ']') : TextInt('<', '>'));
	ExpressionLogging("Patch value end: \"%s\"\n", patch_val_end ? patch_val_end : "NULL");
	if (!patch_val_end) {
		//Bracket error
		return NULL;
	}
	// Increment expr so that the comparisons
	// don't check the opening bracket
	++expr;
	// This would be a lot simpler if there were a variant of
	// patch_opt_get and func_get that took a length parameter
	if (strnicmp(expr, "codecave:", 9) == 0) {
		out->type = VT_DWORD;
		out->i = GetCodecaveAddress(expr, PtrDiffStrlen(patch_val_end, expr), is_relative, data_refs);
	}
	else if (strnicmp(expr, "option:", 7) == 0) {
		expr += 7;
		const patch_val_t* option = GetOptionValue(expr, PtrDiffStrlen(patch_val_end, expr));
		if (option) {
			*out = *option;
		} else {
			out->type = VT_NONE;
		}
	}
	else if (strnicmp(expr, "patch:", 6) == 0) {
		const patch_val_t* patch_test = GetPatchTestValue(expr, PtrDiffStrlen(patch_val_end, expr));
		if (patch_test) {
			*out = *patch_test;
		} else {
			out->type = VT_NONE;
		}
	}
	else if (strnicmp(expr, "cpuid:", 6) == 0) {
		expr += 6;
		out->type = VT_BYTE;
		out->b = GetCPUFeatureTest(expr, PtrDiffStrlen(patch_val_end, expr));
	}
	else {
		out->type = VT_DWORD;
		out->i = GetBPFuncOrRawAddress(expr, PtrDiffStrlen(patch_val_end, expr), is_relative, data_refs);
	}
	return patch_val_end + 1;
};

const char* get_patch_value(const char* expr, patch_val_t* out, x86_reg_t* regs, size_t rel_source) {
	ExpressionLogging("START PATCH VALUE \"%s\"\n", expr);

	const StackSaver data_refs = { regs, rel_source };

	const char *const expr_next = get_patch_value_impl(expr, out, &data_refs);
	if (!expr_next) goto ExpressionError;
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
		expr, out->i, out->i, out->i, expr
	);
	return NULL;
}

static inline const char* CheckCastType(const char* expr, uint8_t* out) {
	switch (const uint8_t c = expr[0] | 0x20) {
		case 'i': case 'u': case 'f':
			if (expr[1] && expr[2]) {
				switch (const uint32_t temp = *(uint32_t*)expr | TextInt(0x20, 0x00, 0x00, 0x00)) {
					case TextInt('f', '3', '2', ')'):
						*out = VT_FLOAT;
						return expr + 4;
					case TextInt('f', '6', '4', ')'):
						*out = VT_DOUBLE;
						return expr + 4;
					case TextInt('u', '1', '6', ')'):
						*out = VT_WORD;
						return expr + 4;
					case TextInt('i', '1', '6', ')'):
						*out = VT_SWORD;
						return expr + 4;
					case TextInt('u', '3', '2', ')'):
						*out = VT_DWORD;
						return expr + 4;
					case TextInt('i', '3', '2', ')'):
						*out = VT_SDWORD;
						return expr + 4;
					case TextInt('u', '6', '4', ')'):
						*out = VT_QWORD;
						return expr + 4;
					case TextInt('i', '6', '4', ')'):
						*out = VT_SQWORD;
						return expr + 4;
					default:
						switch (temp & TextInt(0xFF, 0xFF, 0xFF, '\0')) {
							case TextInt('u', '8', ')'):
								*out = VT_BYTE;
								return expr + 3;
							case TextInt('i', '8', ')'):
								*out = VT_SBYTE;
								return expr + 3;
						}
				}
			}
		default:
			return NULL;
	}
}

static __forceinline const char* is_reg_name(const char* expr, const x86_reg_t *const regs, size_t* out) {

	bool deref;
	expr += !(deref = expr[0] != '&');

	const void* temp;

	switch (uint8_t next_c = expr[1]; uint8_t c = expr[0] | 0x20) {
		case 'e':
			switch (c |
					(next_c | 0x20) << 8 |
					(next_c ? expr[2] | 0x20 : '\0') << 16) {
				case TextInt('e', 'a', 'x'):	temp = &regs->eax; goto DwordRegister;
				case TextInt('e', 'c', 'x'):	temp = &regs->ecx; goto DwordRegister;
				case TextInt('e', 'd', 'x'):	temp = &regs->edx; goto DwordRegister;
				case TextInt('e', 'b', 'x'):	temp = &regs->ebx; goto DwordRegister;
				case TextInt('e', 's', 'p'):	temp = &regs->esp; goto DwordRegister;
				case TextInt('e', 'b', 'p'):	temp = &regs->ebp; goto DwordRegister;
				case TextInt('e', 's', 'i'):	temp = &regs->esi; goto DwordRegister;
				case TextInt('e', 'd', 'i'):	temp = &regs->edi; goto DwordRegister;
				default:						return NULL;
			}
		case 'a':
			switch (next_c | 0x20) {
				case 'x':	temp = &regs->ax; goto WordRegister;
				case 'l':	temp = &regs->al; goto ByteRegister;
				case 'h':	temp = &regs->ah; goto ByteRegister;
				default:	return NULL;
			}
		case 'c':
			switch (next_c | 0x20) {
				case 'x':	temp = &regs->cx; goto WordRegister;
				case 'l':	temp = &regs->cl; goto ByteRegister;
				case 'h':	temp = &regs->ch; goto ByteRegister;
				default:	return NULL;
			}
		case 'd':
			switch (next_c | 0x20) {
				case 'x':	temp = &regs->dx; goto WordRegister;
				case 'l':	temp = &regs->dl; goto ByteRegister;
				case 'h':	temp = &regs->dh; goto ByteRegister;
				case 'i':	temp = &regs->di; goto WordRegister;
				default:	return NULL;
			}
		case 'b':
			switch (next_c | 0x20) {
				case 'x':	temp = &regs->bx; goto WordRegister;
				case 'l':	temp = &regs->bl; goto ByteRegister;
				case 'h':	temp = &regs->bh; goto ByteRegister;
				case 'p':	temp = &regs->bp; goto WordRegister;
				default:	return NULL;
			}
		case 's':
			switch (next_c | 0x20) {
				case 'p':	temp = &regs->sp; goto WordRegister;
				case 'i':	temp = &regs->si; goto WordRegister;
				default:	return NULL;
			}
		default:
			return NULL;
	}

DwordRegister:
	//if (out) {
		*out = deref ? *(uint32_t*)temp : (size_t)temp;
	//}
	return expr + 3;
WordRegister:
	//if (out) {
		*out = deref ? *(uint16_t*)temp : (size_t)temp;
	//}
	return expr + 2;
ByteRegister:
	//if (out) {
		*out = deref ? *(uint8_t*)temp : (size_t)temp;
	//}
	return expr + 2;
}

static inline const char* PostfixCheck(const char* expr) {
	if ((expr[0] == '+' || expr[0] == '-') && expr[0] == expr[1]) {
		PostIncDecWarningMessage();
		return expr + 2;
	}
	return expr;
}

static const char* __fastcall consume_value_impl(const char* expr, size_t *const out, const StackSaver *const data_refs) {
	// cast is used for both casts and pointer sizes
	patch_val_t cur_value;
	cur_value.type = VT_DWORD;
	//const char* expr_next;

#define breakpoint_test_var data_refs->regs
#define is_breakpoint (breakpoint_test_var)
#define is_binhack (!breakpoint_test_var)

	--expr;
	while (1) {
		switch ((uint8_t)*++expr) {
			// Somehow it ran out of expression string, so stop parsing
			case '\0':
				goto InvalidValueError;
			// Skip whitespace
			case ' ': case '\t': case '\v': case '\f':
				continue;
			// Pointer sizes
			case 'b': case 'B':
				if (strnicmp(expr, "byte ptr", 8) == 0) {
					cur_value.type = VT_BYTE;
					expr += 7;
					continue;
				}
				goto RawValue;
			case 'w': case 'W':
				if (strnicmp(expr, "word ptr", 8) == 0) {
					cur_value.type = VT_WORD;
					expr += 7;
					continue;
				}
				goto InvalidCharacterError;
			case 'd': case 'D':
				if (strnicmp(expr, "dword ptr", 9) == 0) {
					cur_value.type = VT_DWORD;
					expr += 8;
					continue;
				}
				if (strnicmp(expr, "double ptr", 10) == 0) {
					cur_value.type = VT_DOUBLE;
					expr += 9;
					continue;
				}
				goto RawValue;
			case 'f': case 'F':
				if (strnicmp(expr, "float ptr", 9) == 0) {
					cur_value.type = VT_FLOAT;
					expr += 8;
					continue;
				}
				goto RawValue;
			case 'q': case 'Q':
				if (strnicmp(expr, "qword ptr", 9) == 0) {
					cur_value.type = VT_QWORD;
					expr += 8;
					continue;
				}
				goto InvalidCharacterError;
			// Unary Operators
			case '!': case '~': case '+': case '-': {
				const char* expr_next = consume_value_impl(expr + 1 + (expr[0] == expr[1]), out, data_refs);
				if (!expr_next) goto InvalidValueError;
				//if (out) {
					switch ((uint8_t)expr[0] << (uint8_t)(expr[0] == expr[1])) {
						case '~': *out = ~*out; break;
						case '!': *out = !*out; break;
						case '-': *out *= -1; break;
						/*case '+': *out = +*out; break;*/
						/*case '~' << 1: *out = ~~*out; break;*/
						case '!' << 1: *out = (bool)*out; break;
						case '-' << 1: IncDecWarningMessage(); --*out; break;
						case '+' << 1: IncDecWarningMessage(); ++*out; break;
					}
				//}
				return PostfixCheck(expr_next);
			}
			case '*': {
				// expr + 1 is used to avoid creating a loop
				const char* expr_next = consume_value_impl(expr + 1, out, data_refs);
				if (!expr_next) goto InvalidValueError;
				//if (out) {
					if (!*out) goto NullDerefWarning;
					switch (cur_value.type) {
						case VT_BOOL: *out = (bool)*(uint8_t*)*out; break;
						case VT_BYTE: *out = *(uint8_t*)*out; break;
						case VT_SBYTE: *out = *(int8_t*)*out; break;
						case VT_WORD: *out = *(uint16_t*)*out; break;
						case VT_SWORD: *out = *(int16_t*)*out; break;
						case VT_DWORD: *out = *(uint32_t*)*out; break;
						case VT_SDWORD: *out = *(int32_t*)*out; break;
						case VT_QWORD: *out = (uint32_t)*(uint64_t*)*out; break;
						case VT_SQWORD: *out = (uint32_t)*(int64_t*)*out; break;
						case VT_FLOAT: *out = (uint32_t)*(float*)*out; break;
						case VT_DOUBLE: *out = (uint32_t)*(double*)*out; break;
					}
				//}
				return PostfixCheck(expr_next);
			}
			// Casts and subexpression values
			case '(': {
				// expr + 1 is used to avoid creating a loop
				++expr;
				const char* expr_next = CheckCastType(expr, &cur_value.type);
				if (expr_next) {
					ExpressionLogging("Cast success\n");
					// Casts
					expr_next = consume_value_impl(expr_next, out, data_refs);
					if (!expr_next) goto InvalidValueError;
					++expr_next;
					//if (out) {
						switch (cur_value.type) {
							case VT_BYTE: *out = *(uint8_t*)out; break;
							case VT_SBYTE: *out = *(int8_t*)out; break;
							case VT_WORD: *out = *(uint16_t*)out; break;
							case VT_SWORD: *out = *(int16_t*)out; break;
							case VT_DWORD: *out = *(uint32_t*)out; break;
							case VT_SDWORD: *out = *(int32_t*)out; break;
							//case VT_QWORD: *out = *(uint64_t*)out; break;
							//case VT_SQWORD: *out = *(int64_t*)out; break;
							case VT_FLOAT: *out = (uint32_t)*(float*)out; break;
							// case VT_DOUBLE: *out = (uint64_t)*(double*)out; break;
						}
					//}
				}
				else {
					// Subexpressions
					expr_next = eval_expr_new_impl(expr, ')', out, StartNoOp, 0, data_refs);
					if (!expr_next) goto InvalidExpressionError;
					++expr_next;
				}
				return PostfixCheck(expr_next);
			}
			// Patch value and/or dereference
			case '[':
				if (is_breakpoint) {
					// Dereference
			case '{':
					// expr + 1 is used to avoid creating a loop
					//expr_next = eval_expr_new_impl(expr + 1, c == '[' ? ']' : '}', current_addr, StartNoOp, 0, data_refs);
					const char* expr_next = eval_expr_new_impl(expr + 1, expr[0] == '[' ? ']' : '}', out, StartNoOp, 0, data_refs);
					if (!expr_next) goto InvalidExpressionError;
					++expr_next;
					//if (out) {
						if (!*out) goto NullDerefWarning;
						switch (cur_value.type) {
							case VT_BOOL: *out = (bool)*(uint8_t*)*out; break;
							case VT_BYTE: *out = *(uint8_t*)*out; break;
							case VT_SBYTE: *out = *(int8_t*)*out; break;
							case VT_WORD: *out = *(uint16_t*)*out; break;
							case VT_SWORD: *out = *(int16_t*)*out; break;
							case VT_DWORD: *out = *(uint32_t*)*out; break;
							case VT_SDWORD: *out = *(int32_t*)*out; break;
							case VT_QWORD: *out = (uint32_t)*(uint64_t*)*out; break;
							case VT_SQWORD: *out = (uint32_t)*(int64_t*)*out; break;
							case VT_FLOAT: *out = (uint32_t)*(float*)*out; break;
							case VT_DOUBLE: *out = (uint32_t)*(double*)*out; break;
						}
					//}
					return PostfixCheck(expr_next);
				}
				[[fallthrough]];
			// Guaranteed patch value
			case '<': {
				// DON'T use expr + 1 since that kills get_patch_value
				const char* expr_next = get_patch_value_impl(expr, &cur_value, data_refs);
				if (!expr_next) goto InvalidPatchValueError;
				//if (out) {
					switch (cur_value.type) {
						case VT_BYTE: *out = (uint32_t)cur_value.b; break;
						case VT_SBYTE: *out = (uint32_t)cur_value.sb; break;
						case VT_WORD: *out = (uint32_t)cur_value.w; break;
						case VT_SWORD: *out = (uint32_t)cur_value.sw; break;
						case VT_DWORD: *out = (uint32_t)cur_value.i; break;
						case VT_SDWORD: *out = (uint32_t)cur_value.si; break;
						case VT_QWORD: *out = (uint32_t)cur_value.q; break;
						case VT_SQWORD: *out = (uint32_t)cur_value.sq; break;
						case VT_FLOAT: *out = (uint32_t)cur_value.f; break;
						case VT_DOUBLE: *out = (uint32_t)cur_value.d; break;
						case VT_STRING: *out = (uint32_t)cur_value.str.ptr; break;
						case VT_WSTRING: *out = (uint32_t)cur_value.wstr.ptr; break;
						//case VT_CODE: InvalidCodeOptionWarningMessage(); current = 0; break;
					}
					ExpressionLogging("Parsed patch value is %X / %d / %u\n", *out, *out, *out);
				//}
				return PostfixCheck(expr_next);
			}
			default:
				/*if (expr[0] == end) {
					return expr;
				}*/
			// Raw value or breakpoint register
			case '&':
				if (is_breakpoint) {
					const char* expr_next = is_reg_name(expr, data_refs->regs, out);
					if (expr_next) {
						//if (out) {
							// current is set inside is_reg_name if a register is detected
							ExpressionLogging("Register value was %X / %d / %u\n", *out, *out, *out);
						//}
						return PostfixCheck(expr_next);
					}
				}
				{
RawValue:
					size_t current = str_address_value(expr, nullptr, &cur_value.addr_ret);
					const char* expr_next = cur_value.addr_ret.endptr;
					if (expr == expr_next || (cur_value.addr_ret.error && cur_value.addr_ret.error != STR_ADDRESS_ERROR_GARBAGE)) {
						goto InvalidCharacterError;
					}
					
					//if (out) {
						ExpressionLogging(
							"Raw value was %X / %d / %u\n"
							"Remaining after raw value: \"%s\"\n",
							current, current, current, expr_next
						);
						*out = current;
					//}
					return PostfixCheck(expr_next);
				}
		}
	}

InvalidValueError:
	InvalidValueErrorMessage(expr);
	return NULL;
InvalidExpressionError:
	ExpressionErrorMessage();
	return NULL;
InvalidPatchValueError:
	ValueBracketErrorMessage();
	return NULL;
InvalidCharacterError:
	BadCharacterErrorMessage();
	return NULL;
NullDerefWarning:
	NullDerefWarningMessage();
	return expr;
}

static inline const char* __fastcall skip_value(const char* expr, const char end) {
	--expr;
	int depth = 0;
	while(1) {
		switch (char c = *++expr) {
			case '\0':
				if (end == '\0' && depth == 0) {
					return expr;
				}
				return NULL;
			default:
				if (c == end && depth == 0) {
					return expr + 1;
				}
				continue;
			case '(': case '[': case '{':
				++depth;
				continue;
			case ')': case ']': case '}':
				if (c == end && depth == 0) {
					return expr;
				}
				else if (--depth < 0) {
					return NULL;
				}
				continue;
		}
	}
}

static size_t expr_index = 0;

static const char* __fastcall eval_expr_new_impl(const char* expr, char end, size_t *const out, const op_t start_op, const size_t start_value, const StackSaver *volatile data_refs) {

	ExpressionLogging(
		"START SUBEXPRESSION #%zu: \"%s\" with end \"%hhX\"\n"
		"Current value: %X / %d / %u\n",
		++expr_index, expr, end, start_value, start_value, start_value
	);

	size_t value = start_value;
	op_t ops_cur = start_op;
	op_t ops_next;
	size_t cur_value = 0;

	// Yes, this loop is awful looking, but it's intentional
	do {
		ExpressionLogging(
			"Remaining expression: \"%s\"\n"
			"Current character: %hhX\n",
			expr, expr[0]
		);

		if (ops_cur != NullOp) {
			const char* expr_next_val = consume_value_impl(expr, &cur_value, data_refs);
			if (!expr_next_val) goto InvalidValueError;
			expr = expr_next_val;
		}

		const char* expr_next_op = find_next_op_impl(expr, &ops_next);
		ExpressionLogging(
			"Found operator %hhX: \"%s\"\n"
			"Remaining after op: \"%s\"\n",
			ops_next, PrintOp(ops_next), expr_next_op
		);

		// Encountering an operator with a higher precedence can
		// be solved by recursing eval_expr over the remaining text
		// and treating the result as a single value
		switch (const uint8_t cur_prec = OpData.Precedence[ops_cur],
							  next_prec = OpData.Precedence[ops_next];
				(int8_t)((cur_prec > next_prec) - (cur_prec < next_prec))) {
			default:
				expr = expr_next_op;
				ExpressionLogging("\tLOWER PRECEDENCE than %s\n", PrintOp(ops_next));
				switch (ops_next) {
					default:
						expr = eval_expr_new_impl(expr, end, &cur_value, ops_next, cur_value, data_refs);
						if (!expr) goto InvalidExpressionError;
						ExpressionLogging(
							"\tRETURN FROM SUBEXPRESSION\n"
							"\tRemaining: \"%s\"\n",
							expr
						);
						if (expr[0] == '?') {
					case TernaryConditional:
							if (cur_value) {
								ExpressionLogging("Ternary TRUE compare: %s has < precedence\n", PrintOp(ops_cur));
								if (expr++[1] != ':') {
									expr = eval_expr_new_impl(expr, ':', &cur_value, StartNoOp, 0, data_refs);
									if (!expr) goto InvalidExpressionError;
								}
								ExpressionLogging("Skipping value until %hhX in \"%s\"...\n", end, expr);
								expr = skip_value(expr, end);
								if (!expr) goto InvalidExpressionError;
								ExpressionLogging(
									"Skipping completed\n"
									"Ternary TRUE remaining: \"%s\" with end \"%hhX\"\n",
									expr, end
								);
							}
							else {
								ExpressionLogging(
									"Ternary FALSE compare: %s has < precedence\n"
									"Skipping value until %hhX in \"%s\"...\n",
									PrintOp(ops_cur), ':', expr
								);
								expr = find_matching_end(expr, TextInt('?', ':'));
								if (!expr) goto InvalidExpressionError;
								++expr;
								ExpressionLogging(
									"Skipping completed\n"
									"Ternary FALSE remaining: \"%s\" with end \"%hhX\"\n",
									expr, end
								);
								continue;
							}
						}
						else if (expr[0] != end) {
							ops_next = NullOp;
						}
				}
				break;
			case SameAsNext:
				expr = expr_next_op;
				ExpressionLogging(
					"\tSAME PRECEDENCE with end \"%hhX\"\n",
					end
				);
				// This really should be looking at an actual associativity
				// property instead, but everything I tried resulted in
				// VS doubling the allocated stack space per call, which
				// is pretty terrible for a recursive function. Screw that.
				if ((uint8_t)(cur_prec - OpData.Precedence[Assign]) <= (OpData.Precedence[NullCoalescing] - OpData.Precedence[Assign])) {
					expr = eval_expr_new_impl(expr, end, &cur_value, ops_next, cur_value, data_refs);
					if (!expr) goto InvalidExpressionError;
				}
				/*switch (cur_prec) {
					case OpData.Precedence[NullCoalescing]:
					case OpData.Precedence[TernaryConditional]:
					case OpData.Precedence[Assign]:
						expr = eval_expr_new_impl(expr, end, &cur_value, ops_next, cur_value, data_refs);
						if (!expr) goto InvalidExpressionError;
				}*/
				/*if (OpData.Associativity[ops_cur] == RightAssociative) {
					expr = eval_expr_new_impl(expr, end, &cur_value, ops_next, cur_value, data_refs);
					if (!expr) goto InvalidExpressionError;
				}*/
				break;
			case HigherThanNext:
				ExpressionLogging("\tHIGHER PRECEDENCE than %s\n", PrintOp(ops_next));
				// An op with a lower precedence can only be encountered when it's
				// necessary to end the current sub-expression, so apply the
				// current operator and then exit
				end = expr[0];
		}

		ExpressionLogging("Applying operation: \"%u %s %u\"\n", value, PrintOp(ops_cur), cur_value);
		value = ApplyOperator(value, cur_value, ops_cur);
		ExpressionLogging("Result of operator was %X / %d / %u\n", value, value, value);
		ops_cur = ops_next;

	} while (expr[0] != end);
	ExpressionLogging(
		"END SUBEXPRESSION #%zu: \"%s\" + %hhu\n"
		"Subexpression value was %X / %d / %u\n",
		expr_index--, expr, 0, value, value, value
	);
	if (out) *out = value;
	return expr;

InvalidValueError:
	InvalidValueErrorMessage(expr);
	return NULL;
InvalidExpressionError:
	ExpressionErrorMessage();
	return NULL;
}

const char* __fastcall eval_expr(const char* expr, char end, size_t* out, x86_reg_t* regs, size_t rel_source) {
	expr_index = 0;
	ExpressionLogging("START EXPRESSION \"%s\" with end \"%hhX\"\n", expr, end);

	const StackSaver data_refs = { regs, rel_source };

	const char *const expr_next = eval_expr_new_impl(expr, end, out, StartNoOp, 0, &data_refs);
	if (!expr_next) goto ExpressionError;
	ExpressionLogging(
		"END EXPRESSION\n"
		"\t\t\t\t\t\t\t\t\t\t\t\t\tExpression was: \"%s\"\n"
		"\t\t\t\t\t\t\t\t\t\t\t\t\tFINAL result was: %X / %d / %u\n"
		"\t\t\t\t\t\t\t\t\t\t\t\t\tRemaining after final: \"%s\"\n",
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
		"\t\t\t\t\t\t\t\t\t\t\t\t\tRemaining after final: \"%s\"\n",
		expr, *out, *out, *out, expr
	);
	return expr;
}
