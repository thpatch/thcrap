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
		long* const data0 = (long*)&data[0];
		long* const data1 = (long*)&data[1];
		long* const data2 = (long*)&data[2];
		long* const data3 = (long*)&data[3];
		switch (data[0]) {
			default: //case 7:
				__cpuidex(data, 7, 0);
				HasBMI1				= _bittest(data1, 3);
				HasAVX2				= _bittest(data1, 4);
				HasERMS				= _bittest(data1, 9);
				HasAVX512F			= _bittest(data1, 16);
				HasAVX512DQ			= _bittest(data1, 17);
				HasADX				= _bittest(data1, 19);
				HasAVX512IFMA		= _bittest(data1, 21);
				HasAVX512PF			= _bittest(data1, 26);
				HasAVX512ER			= _bittest(data1, 27);
				HasAVX512CD			= _bittest(data1, 28);
				HasAVX512BW			= _bittest(data1, 30);
				HasAVX512VL			= _bittest(data1, 31);
				HasAVX512VBMI		= _bittest(data2, 1);
				HasAVX512VBMI2		= _bittest(data2, 6);
				HasGFNI				= _bittest(data2, 8);
				HasVPCLMULQDQ		= _bittest(data2, 10);
				HasAVX512VNNI		= _bittest(data2, 11);
				HasAVX512BITALG		= _bittest(data2, 12);
				HasAVX512VPOPCNTDQ	= _bittest(data2, 14);
				HasAVX5124VNNIW		= _bittest(data3, 2);
				HasAVX5124FMAPS		= _bittest(data3, 3);
				HasFSRM				= _bittest(data3, 4);
				HasAVX512VP2I		= _bittest(data3, 8);
				switch (data[0]) {
					default: //case 1:
						__cpuidex(data, 7, 1);
						HasAVX512BF16 = _bittest(data0, 5);
					case 0:;
				}
			case 6: case 5: case 4: case 3: case 2:
				__cpuid(data, 0x80000000);
				if (data[0] >= 0x80000001) {
					HasMMXEXT		= _bittest(data3, 22);
					Has3DNOWEXT		= _bittest(data3, 30);
					Has3DNOW		= _bittest(data3, 31);
					HasABM			= _bittest(data2, 5);
					HasSSE4A		= _bittest(data2, 6);
					HasXOP			= _bittest(data2, 7);
					HasFMA4			= _bittest(data2, 16);
					HasTBM			= _bittest(data2, 21);
				}
			case 1:
				__cpuid(data, 1);
				HasCMPXCHG8			= _bittest(data3, 8);
				HasCMOV				= _bittest(data3, 15);
				HasMMX				= _bittest(data3, 23);
				HasFXSAVE			= _bittest(data3, 24);
				HasSSE				= _bittest(data3, 25);
				HasSSE2				= _bittest(data3, 26);
				HasSSE3				= _bittest(data2, 1);
				HasPCLMULQDQ		= _bittest(data2, 1);
				HasSSSE3			= _bittest(data2, 9);
				HasFMA				= _bittest(data2, 12);
				HasCMPXCHG16B		= _bittest(data2, 13);
				HasSSE41			= _bittest(data2, 19);
				HasSSE42			= _bittest(data2, 20);
				HasMOVBE			= _bittest(data2, 22);
				HasPOPCNT			= _bittest(data2, 23);
				HasAVX				= _bittest(data2, 28);
				HasF16C				= _bittest(data2, 29);
			case 0:;
		}
	}
};

static const CPUID_Data_t CPUID_Data;

#define WarnOnce(warning) do {\
	static bool AlreadyDisplayedWarning = false;\
	if (!AlreadyDisplayedWarning) { \
		warning; \
		AlreadyDisplayedWarning = true; \
	}\
} while (0)

static __declspec(noinline) void IncDecWarningMessage(void) {
	WarnOnce(log_print("EXPRESSION WARNING 0: Prefix increment and decrement operators do not currently function as expected because it is not possible to modify the value of an option in an expression. These operators only function to add one to a value, but do not actually modify it.\n"));
}

static __declspec(noinline) void AssignmentWarningMessage(void) {
	WarnOnce(log_print("EXPRESSION WARNING 1: Assignment operators do not currently function as expected because it is not possible to modify the value of an option in an expression. These operators are only included for future compatibility and operator precedence reasons.\n"));
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
static __declspec(noinline) void CodecaveNotFoundWarningMessage(const char *const name, size_t name_length) {
	if (!DisableCodecaveNotFound) {
		log_printf("EXPRESSION WARNING 3: Codecave \"%.*s\" not found! Returning NULL...\n", name_length, name);
	}
}

static __declspec(noinline) void PostIncDecWarningMessage(void) {
	WarnOnce(log_print("EXPRESSION WARNING 4: Postfix increment and decrement operators do not currently function as expected because it is not possible to modify the value of an option in an expression. These operators do nothing and are only included for future compatibility and operator precedence reasons.\n"));
}

static __declspec(noinline) void InvalidCPUFeatureWarningMessage(const char* name, size_t name_length) {
	log_printf("EXPRESSION WARNING 5: Unknown CPU feature \"%.*s\"! Assuming feature is present and returning 1...\n", name_length, name);
}

static __declspec(noinline) void InvalidCodeOptionWarningMessage(void) {
	log_print("EXPRESSION WARNING 6: Code options are not valid in expressions! Returning NULL...\n");
}

static __declspec(noinline) void NullDerefWarningMessage(void) {
	log_print("EXPRESSION WARNING 7: Attempted to dereference NULL value! Returning NULL...\n");
}

static __declspec(noinline) void ExpressionErrorMessage(void) {
	log_print("EXPRESSION ERROR: Error parsing expression!\n");
}

static __declspec(noinline) void GroupingBracketErrorMessage(void) {
	log_print("EXPRESSION ERROR 0: Unmatched grouping brackets\n");
}

static __declspec(noinline) void ValueBracketErrorMessage(void) {
	log_print("EXPRESSION ERROR 1: Unmatched patch value brackets\n");
}

static __declspec(noinline) void BadCharacterErrorMessage(void) {
	log_print("EXPRESSION ERROR 2: Unknown character\n");
}

static __declspec(noinline) void OptionNotFoundErrorMessage(const char* name, size_t name_length) {
	log_printf("EXPRESSION ERROR 3: Option \"%.*s\" not found\n", name_length, name);
}

static __declspec(noinline) void InvalidValueErrorMessage(const char *const str) {
	log_printf("EXPRESSION ERROR 4: Invalid value \"%s\"\n", str);
}

typedef uint8_t op_t;

typedef struct {
	const x86_reg_t *const regs;
	const size_t rel_source;
} StackSaver;

static const char* eval_expr_new_impl(const char* expr, const char end, size_t *const out, const op_t start_op, const size_t start_value, const StackSaver *const data_refs);
static const char* consume_value_impl(const char* expr, size_t *const out, const StackSaver *const data_refs);

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
	CircularLeftShift = PreventOverlap('r') + '<' + '<',
	CircularRightShift = PreventOverlap('r') + '>' + '>',
	ThreeWay = '<' + '=' + '>',
	Less = '<',
	LessEqual = '<' + '=',
	Greater = '>',
	GreaterEqual = '>' + '=',
	Equal = '=' + '=',
	NotEqual = PreventOverlap('!') + '=',
	BitwiseAnd = PreventOverlap('&'),
	BitwiseNand = '~' + BitwiseAnd,
	BitwiseXor = '^',
	BitwiseXnor = '~' + BitwiseXor,
	BitwiseOr = PreventOverlap('|'),
	BitwiseNor = '~' + BitwiseOr,
	LogicalAnd = PreventOverlap('&') + PreventOverlap('&'),
	LogicalAndSC = LogicalAnd - 1,
	LogicalNand = PreventOverlap('!') + LogicalAnd,
	LogicalNandSC = LogicalNand - 1,
	LogicalXor = '^' + '^',
	LogicalXnor = PreventOverlap('!') + LogicalXor,
	LogicalOr = PreventOverlap('|') + PreventOverlap('|'),
	LogicalOrSC = LogicalOr - 1,
	LogicalNor = PreventOverlap('!') + LogicalOr,
	LogicalNorSC = LogicalNor - 1,
	TernaryConditional = PreventOverlap('?'),
	Assign = '=',
	AddAssign = Add + '=',
	SubtractAssign = Subtract + '=',
	MultiplyAssign = Multiply + '=',
	DivideAssign = Divide + '=',
	ModuloAssign = Modulo + '=',
	ArithmeticLeftShiftAssign = ArithmeticLeftShift + '=',
	ArithmeticRightShiftAssign = ArithmeticRightShift + '=',
	LogicalLeftShiftAssign = LogicalLeftShift + '=',
	LogicalRightShiftAssign = LogicalRightShift + '=',
	CircularLeftShiftAssign = CircularLeftShift + '=',
	CircularRightShiftAssign = CircularRightShift + '=',
	AndAssign = BitwiseAnd + '=',
	NandAssign = BitwiseNand + '=',
	XorAssign = BitwiseXor + '=',
	XnorAssign = BitwiseXnor + '=',
	OrAssign = BitwiseOr + '=',
	NorAssign = BitwiseNor + '=',
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
		Precedence[LogicalLeftShift] = Precedence[LogicalRightShift] = Precedence[ArithmeticLeftShift] = Precedence[ArithmeticRightShift] = Precedence[CircularLeftShift] = Precedence[CircularRightShift] = SHIFT_PRECEDENCE;
		Associativity[LogicalLeftShift] = Associativity[LogicalRightShift] = Associativity[ArithmeticLeftShift] = Associativity[ArithmeticRightShift] = Associativity[CircularLeftShift] = Associativity[CircularRightShift] = SHIFT_ASSOCIATIVITY;
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
		Precedence[LogicalAnd] = Precedence[LogicalNand] = Precedence[LogicalAndSC] = Precedence[LogicalNandSC] = AND_PRECEDENCE;
		Associativity[LogicalAnd] = Associativity[LogicalNand] = Associativity[LogicalAndSC] = Associativity[LogicalNandSC] = AND_ASSOCIATIVITY;
#define XOR_PRECEDENCE 7
#define XOR_ASSOCIATIVITY LeftAssociative
		Precedence[LogicalXor] = Precedence[LogicalXnor] = XOR_PRECEDENCE;
		Associativity[LogicalXor] = Associativity[LogicalXnor] = XOR_ASSOCIATIVITY;
#define OR_PRECEDENCE 6
#define OR_ASSOCIATIVITY LeftAssociative
		Precedence[LogicalOr] = Precedence[LogicalNor] = Precedence[LogicalOrSC] = Precedence[LogicalNorSC] = OR_PRECEDENCE;
		Associativity[LogicalOr] = Associativity[LogicalNor] = Associativity[LogicalOrSC] = Associativity[LogicalNorSC] = OR_ASSOCIATIVITY;
#define TERNARY_PRECEDENCE 5
#define TERNARY_ASSOCIATIVITY RightAssociative
		Precedence[TernaryConditional] = TERNARY_PRECEDENCE;
		Associativity[TernaryConditional] = TERNARY_ASSOCIATIVITY;
#define ASSIGN_PRECEDENCE 4
#define ASSIGN_ASSOCIATIVITY RightAssociative
		Precedence[Assign] = Precedence[AddAssign] = Precedence[SubtractAssign] = Precedence[MultiplyAssign] = Precedence[DivideAssign] = Precedence[ModuloAssign] = Precedence[LogicalLeftShiftAssign] = Precedence[LogicalRightShiftAssign] = Precedence[ArithmeticLeftShiftAssign] = Precedence[ArithmeticRightShiftAssign] = Precedence[CircularLeftShiftAssign] = Precedence[CircularRightShiftAssign] = Precedence[AndAssign] = Precedence[OrAssign] = Precedence[XorAssign] = Precedence[NandAssign] = Precedence[XnorAssign] = Precedence[NorAssign] = ASSIGN_PRECEDENCE;
		Associativity[Assign] = Associativity[AddAssign] = Associativity[SubtractAssign] = Associativity[MultiplyAssign] = Associativity[DivideAssign] = Associativity[ModuloAssign] = Associativity[LogicalLeftShiftAssign] = Associativity[LogicalRightShiftAssign] = Associativity[ArithmeticLeftShiftAssign] = Associativity[ArithmeticRightShiftAssign] = Associativity[CircularLeftShiftAssign] = Associativity[CircularRightShiftAssign] = Associativity[AndAssign] = Associativity[OrAssign] = Associativity[XorAssign] = Associativity[NandAssign] = Associativity[XnorAssign] = Associativity[NorAssign] = ASSIGN_ASSOCIATIVITY;
#define COMMA_PRECEDENCE 3
#define COMMA_ASSOCIATIVITY LeftAssociative
		Precedence[Comma] = COMMA_PRECEDENCE;
		Associativity[Comma] = COMMA_ASSOCIATIVITY;
#define GOMMA_PRECEDENCE 2
#define GOMMA_ASSOCIATIVITY LeftAssociative
		Precedence[Gomma] = GOMMA_PRECEDENCE;
		Associativity[Gomma] = GOMMA_ASSOCIATIVITY;
#define STARTOP_PRECEDENCE 1
#define STARTOP_ASSOCIATIVITY LeftAssociative
		Precedence[StartNoOp] = STARTOP_PRECEDENCE;
		Associativity[StartNoOp] = STARTOP_ASSOCIATIVITY;
#define NOOP_PRECEDENCE 0
#define NOOP_ASSOCIATIVITY LeftAssociative
		Precedence[NullOp] = Precedence[EndGroupOp] = Precedence[StandaloneTernaryEnd] = NOOP_PRECEDENCE;
		Associativity[NullOp] = Associativity[EndGroupOp] = Associativity[StandaloneTernaryEnd] = NOOP_ASSOCIATIVITY;
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

static __declspec(noinline) const char* find_next_op_impl(const char *const expr, op_t *const out) {
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
			case '?':
				c += c;
				[[fallthrough]];
			case ',': case ';': //case ':':
				goto CRetPlus1;
			case ':':
				*out = c;
				return expr_ref;
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

static __declspec(noinline) const patch_val_t* GetOptionValue(const char* name, size_t name_length) {
	ExpressionLogging("Option: \"%.*s\"\n", name_length, name);
	const patch_val_t* const option = patch_opt_get_len(name, name_length);
	if (!option) {
		OptionNotFoundErrorMessage(name, name_length);
	}
	return option;
}

static __declspec(noinline) const patch_val_t* GetPatchTestValue(const char* name, size_t name_length) {
	ExpressionLogging("PatchTest: \"%.*s\"\n", name_length, name);
	const patch_val_t* const patch_test = patch_opt_get_len(name, name_length);
	return patch_test;
}

static __declspec(noinline) bool GetCPUFeatureTest(const char* name, size_t name_length) {
	ExpressionLogging("CPUFeatureTest: \"%.*s\"\n", name_length, name);
	bool ret = false;
	// Yuck
	switch (name_length) {
		case 15:
			if		(strnicmp(name, "avx512vpopcntdq", name_length) == 0) ret = CPUID_Data.HasAVX512VPOPCNTDQ;
			else	goto InvalidCPUFeatureError;
			break;
		case 12:
			if		(strnicmp(name, "avx512bitalg", name_length) == 0) ret = CPUID_Data.HasAVX512BITALG;
			else if (strnicmp(name, "avx5124fmaps", name_length) == 0) ret = CPUID_Data.HasAVX5124FMAPS;
			else if (strnicmp(name, "avx5124vnniw", name_length) == 0) ret = CPUID_Data.HasAVX5124VNNIW;
			else	goto InvalidCPUFeatureError;
			break;
		case 11:
			if		(strnicmp(name, "avx512vbmi1", name_length) == 0) ret = CPUID_Data.HasAVX512VBMI;
			else if (strnicmp(name, "avx512vbmi2", name_length) == 0) ret = CPUID_Data.HasAVX512VBMI2;
			else	goto InvalidCPUFeatureError;
			break;
		case 10:
			if		(strnicmp(name, "cmpxchg16b", name_length) == 0) ret = CPUID_Data.HasCMPXCHG16B;
			else if (strnicmp(name, "vpclmulqdq", name_length) == 0) ret = CPUID_Data.HasVPCLMULQDQ;
			else if (strnicmp(name, "avx512ifma", name_length) == 0) ret = CPUID_Data.HasAVX512IFMA;
			else if (strnicmp(name, "avx512vnni", name_length) == 0) ret = CPUID_Data.HasAVX512VNNI;
			else if (strnicmp(name, "avx512vp2i", name_length) == 0) ret = CPUID_Data.HasAVX512VP2I;
			else if (strnicmp(name, "avx512bf16", name_length) == 0) ret = CPUID_Data.HasAVX512BF16;
			else	goto InvalidCPUFeatureError;
			break;
		case 9:
			if		(strnicmp(name, "pclmulqdq", name_length) == 0) ret = CPUID_Data.HasPCLMULQDQ;
			else	goto InvalidCPUFeatureError;
			break;
		case 8:
			if		(strnicmp(name, "cmpxchg8", name_length) == 0) ret = CPUID_Data.HasCMPXCHG8;
			else if (strnicmp(name, "avx512dq", name_length) == 0) ret = CPUID_Data.HasAVX512DQ;
			else if (strnicmp(name, "avx512pf", name_length) == 0) ret = CPUID_Data.HasAVX512PF;
			else if (strnicmp(name, "avx512er", name_length) == 0) ret = CPUID_Data.HasAVX512ER;
			else if (strnicmp(name, "avx512cd", name_length) == 0) ret = CPUID_Data.HasAVX512CD;
			else if (strnicmp(name, "avx512bw", name_length) == 0) ret = CPUID_Data.HasAVX512BW;
			else if (strnicmp(name, "avx512vl", name_length) == 0) ret = CPUID_Data.HasAVX512VL;
			else if (strnicmp(name, "3dnowext", name_length) == 0) ret = CPUID_Data.Has3DNOWEXT;
			else	goto InvalidCPUFeatureError;
			break;
		case 7:
			if		(strnicmp(name, "avx512f", name_length) == 0) ret = CPUID_Data.HasAVX512F;
			else	goto InvalidCPUFeatureError;
			break;
		case 6:
			if		(strnicmp(name, "popcnt", name_length) == 0) ret = CPUID_Data.HasPOPCNT;
			else if (strnicmp(name, "fxsave", name_length) == 0) ret = CPUID_Data.HasFXSAVE;
			else if (strnicmp(name, "mmxext", name_length) == 0) ret = CPUID_Data.HasMMXEXT;
			else	goto InvalidCPUFeatureError;
			break;
		case 5:
			if		(strnicmp(name, "intel", name_length) == 0) ret = CPUID_Data.Manufacturer == Intel;
			else if (strnicmp(name, "ssse3", name_length) == 0) ret = CPUID_Data.HasSSSE3;
			else if (strnicmp(name, "sse41", name_length) == 0) ret = CPUID_Data.HasSSE41;
			else if (strnicmp(name, "sse42", name_length) == 0) ret = CPUID_Data.HasSSE42;
			else if (strnicmp(name, "sse4a", name_length) == 0) ret = CPUID_Data.HasSSE4A;
			else if (strnicmp(name, "movbe", name_length) == 0) ret = CPUID_Data.HasMOVBE;
			else if (strnicmp(name, "3dnow", name_length) == 0) ret = CPUID_Data.Has3DNOW;
			else	goto InvalidCPUFeatureError;
			break;
		case 4:
			if		(strnicmp(name, "cmov", name_length) == 0) ret = CPUID_Data.HasCMOV;
			else if (strnicmp(name, "sse2", name_length) == 0) ret = CPUID_Data.HasSSE2;
			else if (strnicmp(name, "sse3", name_length) == 0) ret = CPUID_Data.HasSSE3;
			else if (strnicmp(name, "avx2", name_length) == 0) ret = CPUID_Data.HasAVX2;
			else if (strnicmp(name, "bmi1", name_length) == 0) ret = CPUID_Data.HasBMI1;
			else if (strnicmp(name, "bmi2", name_length) == 0) ret = CPUID_Data.HasBMI2;
			else if (strnicmp(name, "erms", name_length) == 0) ret = CPUID_Data.HasERMS;
			else if (strnicmp(name, "fsrm", name_length) == 0) ret = CPUID_Data.HasFSRM;
			else if (strnicmp(name, "f16c", name_length) == 0) ret = CPUID_Data.HasF16C;
			else if (strnicmp(name, "gfni", name_length) == 0) ret = CPUID_Data.HasGFNI;
			else if (strnicmp(name, "fma4", name_length) == 0) ret = CPUID_Data.HasFMA4;
			else	goto InvalidCPUFeatureError;
			break;
		case 3:
			if		(strnicmp(name, "amd", name_length) == 0) ret = CPUID_Data.Manufacturer == AMD;
			else if (strnicmp(name, "sse", name_length) == 0) ret = CPUID_Data.HasSSE;
			else if (strnicmp(name, "fma", name_length) == 0) ret = CPUID_Data.HasFMA;
			else if (strnicmp(name, "mmx", name_length) == 0) ret = CPUID_Data.HasMMX;
			else if (strnicmp(name, "avx", name_length) == 0) ret = CPUID_Data.HasAVX;
			else if (strnicmp(name, "adx", name_length) == 0) ret = CPUID_Data.HasADX;
			else if (strnicmp(name, "abm", name_length) == 0) ret = CPUID_Data.HasABM;
			else if (strnicmp(name, "xop", name_length) == 0) ret = CPUID_Data.HasXOP;
			else if (strnicmp(name, "tbm", name_length) == 0) ret = CPUID_Data.HasTBM;
			else	goto InvalidCPUFeatureError;
			break;
		default:
InvalidCPUFeatureError:
			InvalidCPUFeatureWarningMessage(name, name_length);
	}
	return ret;
}

static __declspec(noinline) size_t GetCodecaveAddress(const char *const name, const size_t name_length, const bool is_relative, const StackSaver *const data_refs) {
	ExpressionLogging("CodecaveAddress: \"%.*s\"\n", name_length, name);

	const char* user_offset_expr = strchr(name, '+');
	const size_t name_length_before_offset = user_offset_expr ? PtrDiffStrlen(user_offset_expr, name) : name_length;

	size_t cave_addr = func_get_len(name, name_length_before_offset);
	if (!cave_addr) {
		CodecaveNotFoundWarningMessage(name, name_length_before_offset);
	}
	else {
		if (user_offset_expr) {
			++user_offset_expr;
			char* user_offset_expr_next;
			// Try a hex value by default for compatibility
			size_t user_offset_value = strtoul(user_offset_expr, &user_offset_expr_next, 16);
			switch ((bool)(user_offset_expr == user_offset_expr_next)) {
				case true: {
					// If a hex value doesn't work, try a subexpression
					user_offset_expr = strdup_size(user_offset_expr, name_length - name_length_before_offset);
					const bool expression_error = !eval_expr_new_impl(user_offset_expr, '\0', &user_offset_value, StartNoOp, 0, data_refs);
					free((void*)user_offset_expr);
					if (expression_error) {
						ExpressionErrorMessage();
						break;
					}
					[[fallthrough]];
				}
				default:
					cave_addr += user_offset_value;
			}
		}
		if (is_relative) {
			cave_addr -= data_refs->rel_source + sizeof(void*);
		}
	}
	return cave_addr;
}

static __declspec(noinline) size_t GetBPFuncOrRawAddress(const char *const name, const size_t name_length, const bool is_relative, const StackSaver *const data_refs) {
	ExpressionLogging("BPFuncOrRawAddress: \"%.*s\"\n", name_length, name);
	size_t addr = func_get_len(name, name_length);
	switch (addr) {
		case 0: {// Will be null if the name was not a BP function
			const char* const name_buffer = strdup_size(name, name_length);
			const bool expression_error = !eval_expr_new_impl(name_buffer, '\0', &addr, StartNoOp, 0, data_refs);
			free((void*)name_buffer);
			if (expression_error) {
				ExpressionErrorMessage();
				break;
			}
			[[fallthrough]];
		}
		default:
			if (is_relative) {
				addr -= data_refs->rel_source + sizeof(void*);
			}
	}
	return addr;
}

static __declspec(noinline) patch_val_t GetMultibyteNOP(const char *const name, const size_t name_length, const StackSaver *const data_refs) {
	patch_val_t nop_str;
	nop_str.type = PVT_CODE;
	nop_str.str.len = 0;
	const char* const name_buffer = strdup_size(name, name_length);
	eval_expr_new_impl(name_buffer, '\0', &nop_str.str.len, StartNoOp, 0, data_refs);
	free((void*)name_buffer);
	switch (nop_str.str.len) {
		case 0xF: nop_str.str.ptr = CPUID_Data.Manufacturer == AMD ?
										"0F1F80000000000F1F840000000000" :
										"6666666666662E0F1F840000000000"; break;
		case 0xE: nop_str.str.ptr = CPUID_Data.Manufacturer == AMD ?
										"0F1F80000000000F1F8000000000" :
										"66666666662E0F1F840000000000"; break;
		case 0xD: nop_str.str.ptr = CPUID_Data.Manufacturer == AMD ?
										"660F1F4400000F1F8000000000" :
										"666666662E0F1F840000000000"; break;
		case 0xC: nop_str.str.ptr = CPUID_Data.Manufacturer == AMD ?
										"660F1F440000660F1F440000" :
										"6666662E0F1F840000000000"; break;
		case 0xB: nop_str.str.ptr = CPUID_Data.Manufacturer == AMD ?
										"0F1F440000660F1F440000" :
										"66662E0F1F840000000000"; break;
		case 0xA: nop_str.str.ptr =		"662E0F1F840000000000"; break;
		case 0x9: nop_str.str.ptr =		"660F1F840000000000"; break;
		case 0x8: nop_str.str.ptr =		"0F1F840000000000"; break;
		case 0x7: nop_str.str.ptr =		"0F1F8000000000"; break;
		case 0x6: nop_str.str.ptr =		"660F1F440000"; break;
		case 0x5: nop_str.str.ptr =		"0F1F440000"; break;
		case 0x4: nop_str.str.ptr =		"0F1F4000"; break;
		case 0x3: nop_str.str.ptr =		"0F1F00"; break;
		case 0x2: nop_str.str.ptr =		"6690"; break;
		case 0x1: nop_str.str.ptr =		"90"; break;
		default: nop_str.type = PVT_NONE;
	}
	return nop_str;
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
	if (strnicmp(expr, "codecave:", 9) == 0) {
		out->type = PVT_DWORD;
		out->i = GetCodecaveAddress(expr, PtrDiffStrlen(patch_val_end, expr), is_relative, data_refs);
	}
	else if (strnicmp(expr, "option:", 7) == 0) {
		expr += 7;
		const patch_val_t* option = GetOptionValue(expr, PtrDiffStrlen(patch_val_end, expr));
		if (option) {
			*out = *option;
		} else {
			out->type = PVT_NONE;
		}
	}
	else if (strnicmp(expr, "patch:", 6) == 0) {
		const patch_val_t* patch_test = GetPatchTestValue(expr, PtrDiffStrlen(patch_val_end, expr));
		if (patch_test) {
			*out = *patch_test;
		} else {
			out->type = PVT_NONE;
		}
	}
	else if (strnicmp(expr, "cpuid:", 6) == 0) {
		expr += 6;
		out->type = PVT_BYTE;
		out->b = GetCPUFeatureTest(expr, PtrDiffStrlen(patch_val_end, expr));
	}
	else if (strnicmp(expr, "nop:", 3) == 0) {
		expr += 4;
		*out = GetMultibyteNOP(expr, PtrDiffStrlen(patch_val_end, expr), data_refs);
	}
	else {
		out->type = PVT_DWORD;
		out->i = GetBPFuncOrRawAddress(expr, PtrDiffStrlen(patch_val_end, expr), is_relative, data_refs);
	}
	return patch_val_end + 1;
};

const char* get_patch_value(const char* expr, patch_val_t* out, x86_reg_t* regs, size_t rel_source) {
	ExpressionLogging("START PATCH VALUE \"%s\"\n", expr);

	const StackSaver data_refs = { regs, rel_source };

	const char *const expr_next = get_patch_value_impl(expr, out, &data_refs);
	if (expr_next) {
		ExpressionLogging(
			"END PATCH VALUE\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tPatch value was: \"%s\"\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tFINAL result was: %X / %d / %u\n"
			"Remaining after final: \"%s\"\n",
			expr, out->i, out->i, out->i, expr_next
		);
	}
	else {
		ExpressionErrorMessage();
		out->type = PVT_DWORD;
		out->i = 0;
		ExpressionLogging(
			"END PATCH VALUE WITH ERROR\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tPatch value was: \"%s\"\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tFINAL result was: %X / %d / %u\n"
			"Remaining after final: \"%s\"\n",
			expr, out->i, out->i, out->i, expr
		);
	}
	return expr_next;
}

static inline const char* CheckCastType(const char* expr, uint8_t* out) {
	uint8_t type = 0;
	switch (*expr++ & 0xDF) {
		default:
			return NULL;
		case 'I':
			++type;
		case 'U':
			++type;
			switch (*expr++) {
				default:
					return NULL;
				case '6':
					if (*expr++ != '4') return NULL;
					type += 6;
					break;
				case '3':
					if (*expr++ != '2') return NULL;
					type += 4;
					break;
				case '1':
					if (*expr++ != '6') return NULL;
					type += 2;
					break;
				case '8':
					break;
			}
			break;
		case 'F':
			type = PVT_FLOAT;
			switch (*expr++) {
				default:
					return NULL;
				case '8':
					if (*expr++ != '0') return NULL;
					type += 2;
					break;
				case '6':
					if (*expr++ != '4') return NULL;
					type += 1;
					break;
				case '3':
					if (*expr++ != '2') return NULL;
			}
			break;
	}
	if (*expr++ != ')') return NULL;
	*out = type;
	return expr;

	/*switch (const uint8_t c = expr[0] | 0x20) {
		case 'i': case 'u': case 'f':
			if (expr[1] && expr[2]) {
				switch (const uint32_t temp = *(uint32_t*)expr | TextInt(0x20, 0x00, 0x00, 0x00)) {
					case TextInt('f', '3', '2', ')'):
						*out = PVT_FLOAT;
						return expr + 4;
					case TextInt('f', '6', '4', ')'):
						*out = PVT_DOUBLE;
						return expr + 4;
					case TextInt('f', '8', '0', ')'):
						*out = PVT_LONGDOUBLE;
						return expr + 4;
					case TextInt('u', '1', '6', ')'):
						*out = PVT_WORD;
						return expr + 4;
					case TextInt('i', '1', '6', ')'):
						*out = PVT_SWORD;
						return expr + 4;
					case TextInt('u', '3', '2', ')'):
						*out = PVT_DWORD;
						return expr + 4;
					case TextInt('i', '3', '2', ')'):
						*out = PVT_SDWORD;
						return expr + 4;
					case TextInt('u', '6', '4', ')'):
						*out = PVT_QWORD;
						return expr + 4;
					case TextInt('i', '6', '4', ')'):
						*out = PVT_SQWORD;
						return expr + 4;
					default:
						switch (temp & TextInt(0xFF, 0xFF, 0xFF, '\0')) {
							case TextInt('u', '8', ')'):
								*out = PVT_BYTE;
								return expr + 3;
							case TextInt('i', '8', ')'):
								*out = PVT_SBYTE;
								return expr + 3;
						}
				}
			}
		default:
			return NULL;
	}*/
}

static __forceinline const char* is_reg_name(const char* expr, const x86_reg_t *const regs, size_t* out) {

	enum : uint8_t {
		REG_EDI = 0b0000, // 0
		REG_ESI = 0b0001, // 1
		REG_EBP = 0b0010, // 2
		REG_ESP = 0b0011, // 3
		REG_EBX = 0b0100, // 4
		REG_EDX = 0b0101, // 5
		REG_ECX = 0b0110, // 6
		REG_EAX = 0b0111, // 7
	};

	enum : uint8_t {
		REG_DW = 0b00, // 0
		REG_BL = 0b01, // 1
		REG_BH = 0b10, // 2
		REG_W  = 0b11, // 3
	};

	bool deref;
	expr += !(deref = expr[0] != '&');

	uint8_t out_reg = REG_EDI;
	uint8_t letter1 = *expr++ & 0xDF;
	uint8_t out_size = letter1 == 'E' ? letter1 = *expr++ & 0xDF, REG_DW : REG_W;
	uint8_t letter2 = *expr++ & 0xDF;
	switch (letter1) {
		default:
			return NULL;
		case 'S':
			out_reg |= REG_ESI;
		case 'B':
			if (letter2 == 'P') {
				out_reg |= REG_EBP;
				goto RegEnd;
			}
		case 'D':
			if (letter2 == 'I') goto RegEnd;
			out_reg |= letter1 == 'B' ? REG_EBX : REG_EDX;
			break;
		case 'A':
			out_reg |= REG_EAX;
			break;
		case 'C':
			out_reg |= REG_ECX;
	}
	if (letter2 != 'X' && !(out_size &= ((letter2 == 'L') | ((letter2 == 'H') << 1)))) return NULL;
RegEnd:
	union single_reg {
		uint32_t dword;
		uint16_t word;
		uint8_t byte;
	}* temp = (single_reg*)regs + out_reg + 1;
	temp = (single_reg*)((size_t)temp + (out_size == REG_BH));
	if (!out) return expr;
	if (!deref) {
		*out = (size_t)temp;
		return expr;
	}
	switch (out_size) {
		case REG_DW:
			*out = temp->dword;
			return expr;
		case REG_W:
			*out = temp->word;
			return expr;
		default:
			*out = temp->byte;
			return expr;
	}

	/*switch (uint8_t next_c = expr[1]; uint8_t c = expr[0] | 0x20) {
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
	if (out) {
		*out = deref ? *(uint32_t*)temp : (size_t)temp;
	}
	return expr + 3;
WordRegister:
	if (out) {
		*out = deref ? *(uint16_t*)temp : (size_t)temp;
	}
	return expr + 2;
ByteRegister:
	if (out) {
		*out = deref ? *(uint8_t*)temp : (size_t)temp;
	}
	return expr + 2;*/
}

static __forceinline const char* PostfixCheck(const char* expr) {
	if ((expr[0] == '+' || expr[0] == '-') && expr[0] == expr[1]) {
		PostIncDecWarningMessage();
		return expr + 2;
	}
	return expr;
}

static const char* consume_value_impl(const char* expr, size_t *const out, const StackSaver *const data_refs) {
	// cast is used for both casts and pointer sizes
	patch_val_t cur_value;
	cur_value.type = PVT_DWORD;

#define breakpoint_test_var data_refs->regs
#define is_breakpoint (breakpoint_test_var)
#define is_binhack (!breakpoint_test_var)

	--expr;
	while (1) {
		switch ((uint8_t)*++expr) {
			default:
				goto InvalidCharacterError;
			// Raw value or breakpoint register
			case '&':
			case 'a': case 'A': case 'c': case 'C': case 'e': case 'E':
			case 's': case 'S':
			RawValueOrRegister:
				if (is_breakpoint) {
					if (const char* expr_next = is_reg_name(expr, data_refs->regs, out)) {
						if (out) {
							// current is set inside is_reg_name if a register is detected
							ExpressionLogging("Register value was %X / %d / %u\n", *out, *out, *out);
						}
						return PostfixCheck(expr_next);
					}
				}
			case 'r': case 'R':
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
				{
				RawValue:
					size_t current = str_address_value(expr, nullptr, &cur_value.addr_ret);
					const char* expr_next = cur_value.addr_ret.endptr;
					if (expr == expr_next || (cur_value.addr_ret.error && cur_value.addr_ret.error != STR_ADDRESS_ERROR_GARBAGE)) {
						/*if (expr[0] == end) {
							return expr;
						}*/
						goto InvalidCharacterError;
					}
					if (out) {
						ExpressionLogging(
							"Raw value was %X / %d / %u\n"
							"Remaining after raw value: \"%s\"\n",
							current, current, current, expr_next
						);
						*out = current;
					}
					return PostfixCheck(expr_next);
				}
			// Somehow it ran out of expression string, so stop parsing
			case '\0':
				goto InvalidValueError;
			// Skip whitespace
			case ' ': case '\t': case '\v': case '\f':
				continue;
			// Pointer sizes
			case 'b': case 'B':
				if (strnicmp(expr, "byte ptr", 8) == 0) {
					cur_value.type = PVT_BYTE;
					expr += 7;
					continue;
				}
				goto RawValueOrRegister;
			case 'w': case 'W':
				if (strnicmp(expr, "word ptr", 8) == 0) {
					cur_value.type = PVT_WORD;
					expr += 7;
					continue;
				}
				goto InvalidCharacterError;
			case 'd': case 'D':
				if (strnicmp(expr, "dword ptr", 9) == 0) {
					cur_value.type = PVT_DWORD;
					expr += 8;
					continue;
				}
				if (strnicmp(expr, "double ptr", 10) == 0) {
					cur_value.type = PVT_DOUBLE;
					expr += 9;
					continue;
				}
				goto RawValueOrRegister;
			case 'f': case 'F':
				if (strnicmp(expr, "float ptr", 9) == 0) {
					cur_value.type = PVT_FLOAT;
					expr += 8;
					continue;
				}
				goto RawValue;
			case 'q': case 'Q':
				if (strnicmp(expr, "qword ptr", 9) == 0) {
					cur_value.type = PVT_QWORD;
					expr += 8;
					continue;
				}
				goto InvalidCharacterError;
			case 't': case 'T':
				if (strnicmp(expr, "tbyte ptr", 9) == 0) {
					cur_value.type = PVT_LONGDOUBLE;
					expr += 8;
					continue;
				}
				goto InvalidCharacterError;
			// Unary Operators
			case '!': case '~': case '+': case '-': {
				const char* expr_next = consume_value_impl(expr + 1 + (expr[0] == expr[1]), out, data_refs);
				if (expr_next); else goto InvalidValueError;
				if (out) {
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
				}
				return PostfixCheck(expr_next);
			}
			case '*': {
				// expr + 1 is used to avoid creating a loop
				const char* expr_next = consume_value_impl(expr + 1, out, data_refs);
				if (expr_next); else goto InvalidValueError;
				if (out) {
					if (*out); else goto NullDerefWarning;
					if (cur_value.type == PVT_DWORD) {
						*out = *(uint32_t*)*out;
					}
					else {
						switch (cur_value.type) {
							case PVT_BYTE: *out = *(uint8_t*)*out; break;
							case PVT_SBYTE: *out = *(int8_t*)*out; break;
							case PVT_WORD: *out = *(uint16_t*)*out; break;
							case PVT_SWORD: *out = *(int16_t*)*out; break;
							//case PVT_DWORD: *out = *(uint32_t*)*out; break;
							case PVT_SDWORD: *out = *(int32_t*)*out; break;
							case PVT_QWORD: *out = (uint32_t)*(uint64_t*)*out; break;
							case PVT_SQWORD: *out = (uint32_t)*(int64_t*)*out; break;
							case PVT_FLOAT: *out = (uint32_t)*(float*)*out; break;
							case PVT_DOUBLE: *out = (uint32_t)*(double*)*out; break;
							case PVT_LONGDOUBLE: *out = (uint32_t)*(LongDouble80*)*out; break;
						}
					}
				}
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
					if (expr_next); else goto InvalidValueError;
					++expr_next;
					if (out) {
						switch (cur_value.type) {
							case PVT_BYTE: *out = *(uint8_t*)out; break;
							case PVT_SBYTE: *out = *(int8_t*)out; break;
							case PVT_WORD: *out = *(uint16_t*)out; break;
							case PVT_SWORD: *out = *(int16_t*)out; break;
							case PVT_DWORD: *out = *(uint32_t*)out; break;
							case PVT_SDWORD: *out = *(int32_t*)out; break;
							// case PVT_QWORD: *out = *(uint64_t*)out; break;
							// case PVT_SQWORD: *out = *(int64_t*)out; break;
							case PVT_FLOAT: *out = (uint32_t)*(float*)out; break;
							// case PVT_DOUBLE: *out = (uint64_t)*(double*)out; break;
						}
					}
				}
				else {
					// Subexpressions
					expr_next = eval_expr_new_impl(expr, ')', out, StartNoOp, 0, data_refs);
					if (expr_next); else goto InvalidExpressionError;
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
					if (expr_next); else goto InvalidExpressionError;
					++expr_next;
					if (out) {
						if (*out); else goto NullDerefWarning;
						if (cur_value.type == PVT_DWORD) {
							*out = *(uint32_t*)*out;
						}
						else {
							switch (cur_value.type) {
								case PVT_BYTE: *out = *(uint8_t*)*out; break;
								case PVT_SBYTE: *out = *(int8_t*)*out; break;
								case PVT_WORD: *out = *(uint16_t*)*out; break;
								case PVT_SWORD: *out = *(int16_t*)*out; break;
								//case PVT_DWORD: *out = *(uint32_t*)*out; break;
								case PVT_SDWORD: *out = *(int32_t*)*out; break;
								case PVT_QWORD: *out = (uint32_t)*(uint64_t*)*out; break;
								case PVT_SQWORD: *out = (uint32_t)*(int64_t*)*out; break;
								case PVT_FLOAT: *out = (uint32_t)*(float*)*out; break;
								case PVT_DOUBLE: *out = (uint32_t)*(double*)*out; break;
								case PVT_LONGDOUBLE: *out = (uint32_t)*(LongDouble80*)*out; break;
							}
						}
					}
					return PostfixCheck(expr_next);
				}
				[[fallthrough]];
			// Guaranteed patch value
			case '<': {
				// DON'T use expr + 1 since that kills get_patch_value
				const char* expr_next = get_patch_value_impl(expr, &cur_value, data_refs);
				if (expr_next); else goto InvalidPatchValueError;
				if (out) {
					switch (cur_value.type) {
						case PVT_BYTE: *out = (uint32_t)cur_value.b; break;
						case PVT_SBYTE: *out = (uint32_t)cur_value.sb; break;
						case PVT_WORD: *out = (uint32_t)cur_value.w; break;
						case PVT_SWORD: *out = (uint32_t)cur_value.sw; break;
						case PVT_DWORD: *out = (uint32_t)cur_value.i; break;
						case PVT_SDWORD: *out = (uint32_t)cur_value.si; break;
						case PVT_QWORD: *out = (uint32_t)cur_value.q; break;
						case PVT_SQWORD: *out = (uint32_t)cur_value.sq; break;
						case PVT_FLOAT: *out = (uint32_t)cur_value.f; break;
						case PVT_DOUBLE: *out = (uint32_t)cur_value.d; break;
						case PVT_LONGDOUBLE: *out = (uint32_t)cur_value.ld; break;
						case PVT_STRING: *out = (uint32_t)cur_value.str.ptr; break;
						case PVT_STRING16: *out = (uint32_t)cur_value.str16.ptr; break;
						case PVT_STRING32: *out = (uint32_t)cur_value.str32.ptr; break;
						case PVT_CODE: goto InvalidCodeOptionWarning;
					}
					ExpressionLogging("Parsed patch value is %X / %d / %u\n", *out, *out, *out);
				}
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
InvalidCodeOptionWarning:
	InvalidCodeOptionWarningMessage();
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
					return expr;
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

static const char* eval_expr_new_impl(const char* expr, char end, size_t *const out, const op_t start_op, const size_t start_value, const StackSaver *volatile data_refs) {

	ExpressionLogging(
		"START SUBEXPRESSION #%zu: \"%s\" with end \"%hhX\"\n"
		"Current value: %X / %d / %u\n",
		++expr_index, expr, end, start_value, start_value, start_value
	);

	size_t value = start_value;
	op_t ops_cur = start_op;
	op_t ops_next;
	size_t cur_value = 0;
	size_t* cur_value_addr = out ? &cur_value : NULL;

	do {
		ExpressionLogging(
			"Remaining expression: \"%s\"\n"
			"Current character: %hhX\n",
			expr, expr[0]
		);

		if (ops_cur != NullOp) {
			const char* expr_next_val = consume_value_impl(expr, cur_value_addr, data_refs);
			if (expr_next_val); else goto InvalidValueError;
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
						expr = eval_expr_new_impl(expr, end, cur_value_addr, ops_next, cur_value, data_refs);
						if (expr); else goto InvalidExpressionError;
						ExpressionLogging(
							"\tRETURN FROM SUBEXPRESSION\n"
							"\tRemaining: \"%s\"\n",
							expr
						);
						if (expr[0] == '?') {
							++expr;
					case TernaryConditional:
							if (cur_value) {
								ExpressionLogging("Ternary TRUE compare: \"%s\"\n", expr);
								if (expr[0] != ':') {
									expr = eval_expr_new_impl(expr, ':', cur_value_addr, StartNoOp, 0, data_refs);
									if (expr); else goto InvalidExpressionError;
								}
								ExpressionLogging("Skipping value until %hhX in \"%s\"...\n", end, expr);
								expr = skip_value(expr, end);
								if (expr); else goto InvalidExpressionError;
								ExpressionLogging(
									"Skipping completed\n"
									"Ternary TRUE remaining: \"%s\" with end \"%hhX\"\n",
									expr, end
								);
							}
							else {
								ExpressionLogging(
									"Ternary FALSE compare: \"%s\"\n"
									"Skipping value until %hhX in \"%s\"...\n",
									expr, ':', expr
								);
								expr = eval_expr_new_impl(expr, ':', NULL, StartNoOp, 0, data_refs);
								if (expr); else goto InvalidExpressionError;
								while (*expr++ != ':');
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
				if ((uint8_t)(cur_prec - OpData.Precedence[Assign]) <= (OpData.Precedence[TernaryConditional] - OpData.Precedence[Assign])) {
					expr = eval_expr_new_impl(expr, end, cur_value_addr, ops_next, cur_value, data_refs);
					if (expr); else goto InvalidExpressionError;
				}
				/*switch (cur_prec) {
					case OpData.Precedence[TernaryConditional]:
					case OpData.Precedence[Assign]:
						expr = eval_expr_new_impl(expr, end, cur_value_addr, ops_next, cur_value, data_refs);
						if (!expr) goto InvalidExpressionError;
				}*/
				/*if (OpData.Associativity[ops_cur] == RightAssociative) {
					expr = eval_expr_new_impl(expr, end, cur_value_addr, ops_next, cur_value, data_refs);
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
		"END SUBEXPRESSION #%zu: \"%s\"\n"
		"Subexpression value was %X / %d / %u\n",
		expr_index--, expr, value, value, value
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
	if (expr_next) {
		ExpressionLogging(
			"END EXPRESSION\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tExpression was: \"%s\"\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tFINAL result was: %X / %d / %u\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tRemaining after final: \"%s\"\n",
			expr, *out, *out, *out, expr_next
		);
	}
	else {
		ExpressionErrorMessage();
		*out = 0;
		ExpressionLogging(
			"END EXPRESSION WITH ERROR\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tExpression was: \"%s\"\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tFINAL result was: %X / %d / %u\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tRemaining after final: \"%s\"\n",
			expr, *out, *out, *out, expr
		);
	}
	return expr_next;
}

void patch_val_set_op(const char* op_str, patch_val_t* Val) {
	if (op_str) {
		op_t op;
		(void)find_next_op_impl(op_str, &op);
		switch (op) {
			case MultiplyAssign:
			case DivideAssign:
			case ModuloAssign:
			case AddAssign:
			case SubtractAssign:
			case ArithmeticLeftShiftAssign:
			case LogicalLeftShiftAssign:
			case ArithmeticRightShiftAssign:
			case LogicalRightShiftAssign:
			case CircularLeftShiftAssign:
			case CircularRightShiftAssign:
			case AndAssign:
			case NandAssign:
			case XorAssign:
			case XnorAssign:
			case OrAssign:
			case NorAssign:
				op -= '=';
				[[fallthrough]];
			case Multiply:
			case Divide:
			case Modulo:
			case Add:
			case Subtract:
			case ArithmeticLeftShift:
			case LogicalLeftShift:
			case ArithmeticRightShift:
			case LogicalRightShift:
			case CircularLeftShift:
			case CircularRightShift:
			case BitwiseAnd:
			case BitwiseNand:
			case BitwiseXor:
			case BitwiseXnor:
			case BitwiseOr:
			case BitwiseNor:
				Val->merge_op = op;
				return;
		}
	}
	Val->merge_op = Add;
	return;
}

patch_val_t patch_val_not(patch_val_t Val) {
	// It'll be fiiiiiiine, right? (Probably not, but I'll fix it later)
	Val.q = ~Val.q;
	return Val;
}

patch_val_t patch_val_op_str(const char* op_str, patch_val_t Val1, patch_val_t Val2) {
	op_t op;
	if (op_str) {
		(void)find_next_op_impl(op_str, &op);
	}
	else {
		op = Val1.merge_op;
	}
	switch (op) {
		case MultiplyAssign: case Multiply:
			return patch_val_mul(Val1, Val2);
		case DivideAssign: case Divide:
			return patch_val_div(Val1, Val2);
		case ModuloAssign: case Modulo:
			return patch_val_mod(Val1, Val2);
		case AddAssign: case Add:
			return patch_val_add(Val1, Val2);
		case SubtractAssign: case Subtract:
			return patch_val_sub(Val1, Val2);
		case ArithmeticLeftShiftAssign: case ArithmeticLeftShift:
		case LogicalLeftShiftAssign: case LogicalLeftShift:
			return patch_val_shr(Val1, Val2);
		case ArithmeticRightShiftAssign: case ArithmeticRightShift:
		case LogicalRightShiftAssign: case LogicalRightShift:
			return patch_val_shl(Val1, Val2);
		case CircularLeftShiftAssign: case CircularLeftShift:
			return patch_val_rol(Val1, Val2);
		case CircularRightShiftAssign: case CircularRightShift:
			return patch_val_ror(Val1, Val2);
		case AndAssign: case BitwiseAnd:
			return patch_val_and(Val1, Val2);
		case NandAssign: case BitwiseNand:
			return patch_val_not(patch_val_and(Val1, Val2));
		case XorAssign: case BitwiseXor:
			return patch_val_xor(Val1, Val2);
		case XnorAssign: case BitwiseXnor:
			return patch_val_not(patch_val_xor(Val1, Val2));
		case OrAssign: case BitwiseOr:
			return patch_val_or(Val1, Val2);
		case NorAssign: case BitwiseNor:
			return patch_val_not(patch_val_or(Val1, Val2));
		case ThreeWay: case Less: case LessEqual: case Greater: case GreaterEqual: case Equal: case NotEqual: case LogicalAnd: case LogicalNand: case LogicalXor: case LogicalXnor: case LogicalOr: case LogicalNor:
			log_print("Options cannot use logical or comparison operators!\n");
			break;
		case Assign:
			log_print("Options cannot use assignment!\n");
			break;
		case Comma: case Gomma:
			log_print("but why tho\n");
			break;
	}
	{
		patch_val_t bad_ret;
		bad_ret.type = PVT_NONE;
		return bad_ret;
	}
}

#define PatchOpFloatVals(op) \
case PVT_FLOAT:			ret.f = Val1.f op Val2.f; break; \
case PVT_DOUBLE:		ret.d = Val1.d op Val2.d; break; \
case PVT_LONGDOUBLE:	ret.ld = Val1.ld op Val2.ld; break;

#define MakePatchOpFunc(op_name, op) \
patch_val_t patch_val_ ## op_name (patch_val_t Val1, patch_val_t Val2) { \
	patch_val_t ret; \
	switch (ret.type = Val1.type) { \
		case PVT_BYTE:			ret.b = Val1.b op Val2.b; break; \
		case PVT_SBYTE:			ret.sb = Val1.sb op Val2.sb; break; \
		case PVT_WORD:			ret.w = Val1.w op Val2.w; break; \
		case PVT_SWORD:			ret.sw = Val1.sw op Val2.sw; break; \
		case PVT_DWORD:			ret.i = Val1.i op Val2.i; break; \
		case PVT_SDWORD:		ret.si = Val1.si op Val2.si; break; \
		case PVT_QWORD:			ret.q = Val1.q op Val2.q; break; \
		case PVT_SQWORD:		ret.sq = Val1.sq op Val2.sq; break; \
		PatchOpFloatVals(op) \
		default: \
			ret.type = PVT_NONE; \
	} \
	return ret; \
}

MakePatchOpFunc(add, +);
MakePatchOpFunc(sub, -);
MakePatchOpFunc(mul, *);
MakePatchOpFunc(div, /);

#undef PatchOpFloatVals
#define PatchOpFloatVals(op)

MakePatchOpFunc(mod, %);
MakePatchOpFunc(shl, <<);
MakePatchOpFunc(shr, >>);
MakePatchOpFunc(and, &);
MakePatchOpFunc(or, |);
MakePatchOpFunc(xor, ^);

patch_val_t patch_val_rol(patch_val_t Val1, patch_val_t Val2) {
	patch_val_t ret;
	switch (ret.type = Val1.type) {
		case PVT_BYTE:			ret.b = Val1.b << Val2.b | Val1.b >> (sizeof(Val1.b) * CHAR_BIT - Val2.b); break;
		case PVT_SBYTE:			ret.sb = Val1.sb << Val2.sb | Val1.sb >> (sizeof(Val1.sb) * CHAR_BIT - Val2.sb); break;
		case PVT_WORD:			ret.w = Val1.w << Val2.w | Val1.w >> (sizeof(Val1.w) * CHAR_BIT - Val2.w); break;
		case PVT_SWORD:			ret.sw = Val1.sw << Val2.sw | Val1.sw >> (sizeof(Val1.sw) * CHAR_BIT - Val2.sw); break;
		case PVT_DWORD:			ret.i = Val1.i << Val2.i | Val1.i >> (sizeof(Val1.i) * CHAR_BIT - Val2.i); break;
		case PVT_SDWORD:		ret.si = Val1.si << Val2.si | Val1.si >> (sizeof(Val1.si) * CHAR_BIT - Val2.si); break;
		case PVT_QWORD:			ret.q = Val1.q << Val2.q | Val1.q >> (sizeof(Val1.q) * CHAR_BIT - Val2.q); break;
		case PVT_SQWORD:		ret.sq = Val1.sq << Val2.sq | Val1.sq >> (sizeof(Val1.sq) * CHAR_BIT - Val2.sq); break;
		PatchOpFloatVals(op)
		default:
			ret.type = PVT_NONE;
	}
	return ret;
}

patch_val_t patch_val_ror(patch_val_t Val1, patch_val_t Val2) {
	patch_val_t ret;
	switch (ret.type = Val1.type) {
		case PVT_BYTE:			ret.b = Val1.b >> Val2.b | Val1.b << (sizeof(Val1.b) * CHAR_BIT - Val2.b); break;
		case PVT_SBYTE:			ret.sb = Val1.sb >> Val2.sb | Val1.sb << (sizeof(Val1.sb) * CHAR_BIT - Val2.sb); break;
		case PVT_WORD:			ret.w = Val1.w >> Val2.w | Val1.w << (sizeof(Val1.w) * CHAR_BIT - Val2.w); break;
		case PVT_SWORD:			ret.sw = Val1.sw >> Val2.sw | Val1.sw << (sizeof(Val1.sw) * CHAR_BIT - Val2.sw); break;
		case PVT_DWORD:			ret.i = Val1.i >> Val2.i | Val1.i << (sizeof(Val1.i) * CHAR_BIT - Val2.i); break;
		case PVT_SDWORD:		ret.si = Val1.si >> Val2.si | Val1.si << (sizeof(Val1.si) * CHAR_BIT - Val2.si); break;
		case PVT_QWORD:			ret.q = Val1.q >> Val2.q | Val1.q << (sizeof(Val1.q) * CHAR_BIT - Val2.q); break;
		case PVT_SQWORD:		ret.sq = Val1.sq >> Val2.sq | Val1.sq << (sizeof(Val1.sq) * CHAR_BIT - Val2.sq); break;
			PatchOpFloatVals(op)
		default:
			ret.type = PVT_NONE;
	}
	return ret;
}
