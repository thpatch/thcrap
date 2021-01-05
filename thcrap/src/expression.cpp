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

constexpr uint32_t ConstExprTextInt(unsigned char one, unsigned char two = '\0', unsigned char three = '\0', unsigned char four = '\0') {
	return four << 24 | three << 16 | two << 8 | one;
}

static inline char* strndup(const char* source, size_t length) {
	char *const ret = (char *const)malloc(length + 1);
	memcpy(ret, source, length);
	ret[length] = '\0';
	return ret;
}

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
		if (data[1] == ConstExprTextInt('G', 'e', 'n', 'u') &&
			data[3] == ConstExprTextInt('i', 'n', 'e', 'I') &&
			data[2] == ConstExprTextInt('n', 't', 'e', 'l')) {
			Manufacturer = Intel;
		} else if (data[1] == ConstExprTextInt('A', 'u', 't', 'h') &&
				   data[3] == ConstExprTextInt('e', 'n', 't', 'i') &&
				   data[2] == ConstExprTextInt('c', 'A', 'M', 'D')) {
			Manufacturer = AMD;
		} else {
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

#define breakpoint_test_var regs
#define is_breakpoint (breakpoint_test_var)
#define is_binhack (!breakpoint_test_var)

#define WarnOnce(warning) do {\
	static bool AlreadyDisplayedWarning = false;\
	if (!AlreadyDisplayedWarning) { \
		warning; \
		AlreadyDisplayedWarning = true; \
	}\
} while (0)

static inline void IncDecWarningMessage(void) {
	WarnOnce(log_printf("EXPRESSION WARNING 0: Prefix increment and decrement operators do not currently function as expected because it is not possible to modify the value of an option in an expression. These operators only function to add one to a value, but do not actually modify it.\n"));
}

static inline void AssignmentWarningMessage(void) {
	WarnOnce(log_printf("EXPRESSION WARNING 1: Assignment operators do not currently function as expected because it is not possible to modify the value of an option in an expression. These operators are only included for future compatibility and operator precedence reasons.\n"));
}

static inline void TernaryWarningMessage(void) {
	WarnOnce(log_printf("EXPRESSION WARNING 2: Expressions currently don't support ternary operations with correct associativity. However, it is still possible to obtain a correct result with parenthesis.\n"));
}

static inline void PatchValueWarningMessage(const char *const name) {
	log_printf("EXPRESSION WARNING 3: Unknown patch value type \"%s\", using value 0\n", name);
}

static inline void CodecaveNotFoundWarningMessage(const char *const name) {
	log_printf("EXPRESSION WARNING 4: Codecave \"%s\" not found\n", name);
}

static inline void PatchTestNotExistWarningMessage(const char *const name) {
	log_printf("EXPRESSION WARNING 5: Couldn't test for patch \"%s\" because patch testing hasn't been implemented yet. Assuming it exists and returning 1...");
}

static inline void PostIncDecWarningMessage(void) {
	WarnOnce(log_printf("EXPRESSION WARNING 6: Postfix increment and decrement operators do not currently function as expected because it is not possible to modify the value of an option in an expression. These operators do nothing and are only included for future compatibility and operator precedence reasons.\n"));
}

//static inline void CPUTestNotExistWarningMessage(const char *const name) {
//	log_printf("EXPRESSION WARNING 7: Couldn't test for CPU feature \"%s\" because CPU feature testing hasn't been implemented yet. Assuming it exists and returning 1...\n");
//}

static inline void InvalidCPUFeatureWarningMessage(const char *const name) {
	log_printf("EXPRESSION WARNING 7: Unknown CPU feature \"%s\"! Assuming feature is present and returning 1...\n");
}

static inline void ExpressionErrorMessage(void) {
	log_printf("EXPRESSION ERROR: Error parsing expression!\n");
}

static inline void GroupingBracketErrorMessage(void) {
	log_printf("EXPRESSION ERROR 0: Unmatched grouping brackets\n");
}

static inline void ValueBracketErrorMessage(void) {
	log_printf("EXPRESSION ERROR 1: Unmatched patch value brackets\n");
}

static inline void BadCharacterErrorMessage(void) {
	log_printf("EXPRESSION ERROR 2: Unknown character\n");
}

static inline void OptionNotFoundErrorMessage(const char *const name) {
	log_printf("EXPRESSION ERROR 3: Option \"%s\" not found\n", name);
}

static inline void InvalidValueErrorMessage(const char *const str) {
	log_printf("EXPRESSION ERROR 4: Invalid value \"%s\"\n", str);
}

static inline const char* eval_expr_new_impl(const char* expr, size_t &out, char end, x86_reg_t *const regs, size_t rel_source);
static inline const char* consume_value_impl(const char* expr, size_t &out, char end, x86_reg_t *const regs, size_t rel_source);

#define PtrDiffStrlen(end_ptr, start_ptr) ((end_ptr) - (start_ptr))

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
		case ConstExprTextInt('e', 'a', 'x'): return &regs->eax;
		case ConstExprTextInt('e', 'c', 'x'): return &regs->ecx;
		case ConstExprTextInt('e', 'd', 'x'): return &regs->edx;
		case ConstExprTextInt('e', 'b', 'x'): return &regs->ebx;
		case ConstExprTextInt('e', 's', 'p'): return &regs->esp;
		case ConstExprTextInt('e', 'b', 'p'): return &regs->ebp;
		case ConstExprTextInt('e', 's', 'i'): return &regs->esi;
		case ConstExprTextInt('e', 'd', 'i'): return &regs->edi;
	}
	if (endptr) {
		*endptr = regname;
	}
	return NULL;
}

#define PreventOverlap(val) val + val
enum {
	NoOp = 0,
	BadBrackets = 1,
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
	ThreeWay = '<' + '=' + '>',
	Less = '<',
	LessEqual = '<' + '=',
	Greater = '>',
	GreaterEqual = '>' + '=',
	Equal = '=' + '=',
	NotEqual = PreventOverlap('!') + '=',
	BitwiseAnd = PreventOverlap('&'),
	BitwiseXor = '^',
	BitwiseOr = PreventOverlap('|'),
	LogicalAnd = PreventOverlap('&') + PreventOverlap('&'),
	LogicalOr = PreventOverlap('|') + PreventOverlap('|'),
	TernaryConditional = PreventOverlap('?'),
	NullCoalescing = PreventOverlap('?') + ':',
	Assign = '=',
	AddAssign = '+' + '=',
	SubtractAssign = '-' + '=',
	MultiplyAssign = '*' + '=',
	DivideAssign = '/' + '=',
	ModuloAssign = '%' + '=',
	LogicalLeftShiftAssign = '<' + '<' + '=',
	LogicalRightShiftAssign = '>' + '>' + '=',
	ArithmeticLeftShiftAssign = '<' + '<' + '<' + '=',
	ArithmeticRightShiftAssign = '>' + '>' + '>' + '=',
	AndAssign = PreventOverlap('&') + '=',
	XorAssign = '^' + '=',
	OrAssign = PreventOverlap('|') + '=',
	Comma = ','
};
typedef unsigned char op_t;

enum CastType : uint8_t {
	invalid_cast = 0,
	T8_ptr = 1,
	T16_ptr = 2,
	T32_ptr = 4,
	T64_ptr = 8,
	u8_cast,
	u16_cast,
	u32_cast,
	i8_cast,
	i16_cast,
	i32_cast,
	f32_cast
};

static inline const char* find_matching_end(const char* str, char start, char end) {
	if (!str) return NULL;
	size_t depth = 0;
	while (*str) {
		if (*str == start) {
			++depth;
		} else if (*str == end) {
			--depth;
			if (!depth) {
				return str;
			}
		}
		++str;
	}
	return NULL;
};

const char* parse_brackets(const char* str, char opening) {
	const char * str_ref = str - 1;
	bool go_to_end = opening == '\0';
	int32_t paren_count = opening == '(';
	int32_t square_count = opening == '[';
	int32_t curly_count = opening == '{';
	int32_t angle_count = opening == '<';
	char c;
	for (;;) {
		c = *++str_ref;
		if (!c)
			return str_ref;
		paren_count -= c == ')';
		square_count -= c == ']';
		curly_count -= c == '}';
		angle_count -= c == '>';
		if (!go_to_end && !paren_count && !square_count && !curly_count && !angle_count)
			return str_ref;
		else if (paren_count < 0 || square_count < 0 || curly_count < 0 || angle_count < 0)
			return str;
		paren_count += c == '(';
		square_count += c == '[';
		curly_count += c == '{';
		angle_count += c == '<';
	}
}

static inline bool validate_brackets(const char* str) {
	return parse_brackets(str, '\0') != str;
}

static inline const char* __fastcall find_next_op_impl(const char* expr, op_t &out) {
	uint32_t c;
	--expr;
	for (;;) {
		switch (c = *(unsigned char*)++expr) {
			case '\0':
				out = c;
				return expr;
			case '!':
				if (expr[1] == '=') {
					out = c + '=';
					return expr + 1;
				}
				continue;
			case '&': case '|':
				c += c;
			
			case '+': case '-': case '*': case '/': case '%': case '=': case '^': case '?':
				uint32_t temp;
				if (expr[1]) {
					if ((c == '<' || c == '>') && expr[2]) {
						temp = *(uint32_t*)expr;
						// Yeah, the if chain is kind of ugly. However, MSVC doesn't
						// optimize this into a jump table and using a switch adds an
						// extra check for "higher than included" values, which are
						// impossible anyway since they already got filtered out.
						if (temp == ConstExprTextInt('>', '>', '>', '=') ||
							temp == ConstExprTextInt('<', '<', '<', '=')) {
								out = c * 3 + '=';
								return expr + 4;
						}
						if (temp == ConstExprTextInt('<', '=', '>')) {
							out = ThreeWay;
							return expr + 3;
						}
						if (temp == ConstExprTextInt('>', '>', '=') ||
							temp == ConstExprTextInt('<', '<', '=')) {
								out = c * 2 + '=';
								return expr + 3;
						}
						if (temp == ConstExprTextInt('>', '>', '>') ||
							temp == ConstExprTextInt('<', '<', '<')) {
								out = c * 3;
								return expr + 3;
						}
					} else {
						temp = *(uint16_t*)expr;
					}
					if (temp == ConstExprTextInt('>', '>') || temp == ConstExprTextInt('<', '<') ||
						temp == ConstExprTextInt('&', '&') || temp == ConstExprTextInt('|', '|') ||
						temp == ConstExprTextInt('*', '*')) {
							out = c * 2;
							return expr + 2;
					}
					if (temp == ConstExprTextInt('|', '=') || temp == ConstExprTextInt('^', '=') ||
						temp == ConstExprTextInt('%', '=') || temp == ConstExprTextInt('&', '=') ||
						temp == ConstExprTextInt('*', '=') || temp == ConstExprTextInt('+', '=') ||
						temp == ConstExprTextInt('-', '=') || temp == ConstExprTextInt('/', '=')) {
							out = c + '=';
							return expr + 2;
					}
					if (temp == ConstExprTextInt('?', ':')) {
						out = NullCoalescing;
						return expr + 2;
					}
				}
			case ',':
				out = c;
				return expr + 1;
		}
	}
}

static inline unsigned char OpPrecedence(op_t op) {
	switch (op) {
		case NoOp:
			return 17;
		case Power:
			return 16;
		/*case Increment: case Decrement: case LogicalNot: case BitwiseNot: case Dereference: case AddressOf:
			return 16;*/
		case Multiply: case Divide: case Modulo:
			return 14;
		case Add: case Subtract:
			return 13;
		case LogicalLeftShift: case LogicalRightShift: case ArithmeticLeftShift: case ArithmeticRightShift:
			return 12;
		case Less: case LessEqual: case Greater: case GreaterEqual:
			return 11;
		case Equal: case NotEqual:
			return 10;
		case ThreeWay:
			return 9;
		case BitwiseAnd:
			return 8;
		case BitwiseXor:
			return 7;
		case BitwiseOr:
			return 6;
		case LogicalAnd:
			return 5;
		case LogicalOr:
			return 4;
		case TernaryConditional: case NullCoalescing:
			return 3;
		case Assign: case AddAssign: case SubtractAssign: case MultiplyAssign: case DivideAssign: case ModuloAssign: case LogicalLeftShiftAssign: case LogicalRightShiftAssign: case ArithmeticLeftShiftAssign: case ArithmeticRightShiftAssign: case AndAssign: case OrAssign: case XorAssign:
			return 2;
		case Comma:
			return 1;
		default:
			return 0;
	}
}

static inline void PrintOp(op_t op) {
	const char * op_text;
	switch (op) {
		case Power: op_text = "**"; break;
		case NoOp: op_text = "NoOp"; break;
		case Multiply: op_text = "*"; break;
		case Divide: op_text = "/"; break;
		case Modulo: op_text = "%"; break;
		case Add: op_text = "+"; break;
		case Subtract: op_text = "-"; break;
		case ArithmeticLeftShift: op_text = "<<"; break;
		case ArithmeticRightShift: op_text = ">>"; break;
		case LogicalLeftShift: op_text = "<<<"; break;
		case LogicalRightShift: op_text = ">>>"; break;
		case BitwiseAnd: op_text = "&"; break;
		case BitwiseXor: op_text = "^"; break;
		case BitwiseOr: op_text = "|"; break;
		case LogicalAnd: op_text = "&&"; break;
		case LogicalOr: op_text = "||"; break;
		case Less: op_text = "<"; break;
		case LessEqual: op_text = "<="; break;
		case Greater: op_text = ">"; break;
		case GreaterEqual: op_text = ">="; break;
		case Equal: op_text = "=="; break;
		case NotEqual: op_text = "!="; break;
		case ThreeWay: op_text = "<=>"; break;
		case TernaryConditional: op_text = "?"; break;
		case NullCoalescing: op_text = "?:"; break;
		case Assign: op_text = "="; break;
		case AddAssign: op_text = "+="; break;
		case SubtractAssign: op_text = "-="; break;
		case MultiplyAssign: op_text = "*="; break;
		case DivideAssign: op_text = "/="; break;
		case ModuloAssign: op_text = "%="; break;
		case LogicalLeftShiftAssign: op_text = "<<="; break;
		case LogicalRightShiftAssign: op_text = ">>="; break;
		case ArithmeticLeftShiftAssign: op_text = "<<<="; break;
		case ArithmeticRightShiftAssign: op_text = ">>>="; break;
		case AndAssign: op_text = "&="; break;
		case XorAssign: op_text = "^="; break;
		case OrAssign: op_text = "|="; break;
		case Comma: op_text = ","; break;
		default: op_text = "ERROR"; break;
	}
	log_printf("Found operator \"%s\"\n", op_text);
}

static inline char CompareOpPrecedence(op_t PrevOp, op_t NextOp) {
	const unsigned char NextPrecedence = OpPrecedence(NextOp);
	const unsigned char PrevPrecedence = OpPrecedence(PrevOp);
	if (NextPrecedence > PrevPrecedence) {
		return 1;
	} else if (NextPrecedence < PrevPrecedence) {
		return -1;
	} else {
		return 0;
	}
}

static inline size_t ApplyOperator(op_t op, size_t value, size_t arg) {
	switch (op) {
		case Power:
			return pow(value, arg);
		case MultiplyAssign:
			AssignmentWarningMessage();
		case Multiply:
			return value * arg;
		case DivideAssign:
			AssignmentWarningMessage();
		case Divide:
			return value / arg;
		case ModuloAssign:
			AssignmentWarningMessage();
		case Modulo:
			return value % arg;
		case AddAssign:
			AssignmentWarningMessage();
		case Add:
			return value + arg;
		case SubtractAssign:
			AssignmentWarningMessage();
		case Subtract:
			return value - arg;
		case LogicalLeftShiftAssign:
		case ArithmeticLeftShiftAssign:
			AssignmentWarningMessage();
		case LogicalLeftShift:
		case ArithmeticLeftShift:
			return value << arg;
		case LogicalRightShiftAssign:
			AssignmentWarningMessage();
		case LogicalRightShift:
			return value >> arg;
		case ArithmeticRightShiftAssign:
			AssignmentWarningMessage();
		case ArithmeticRightShift:
			return (uint32_t)((int32_t)value >> arg);
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
		case BitwiseAnd:
			return value & arg;
		case BitwiseXor:
			return value ^ arg;
		case BitwiseOr:
			return value | arg;
		case LogicalAnd:
			return value && arg;
		case LogicalOr:
			return value || arg;
		case TernaryConditional:
			TernaryWarningMessage();
			return value;
			break;
		case NullCoalescing:
			TernaryWarningMessage();
			return value ? value : arg;
			break;
		case Assign:
			AssignmentWarningMessage();
		case Comma:
			//why tho
		case NoOp:
		default:
			return arg;
	}
}

static inline size_t GetOptionValue(const char *const name, size_t name_length) {
	const char *const name_buffer = strndup(name, name_length);
	const patch_opt_val_t *const option = patch_opt_get(name_buffer);
	size_t ret = 0;
	if (option) {
		memcpy(&ret, option->val.byte_array, option->size);
	} else {
		OptionNotFoundErrorMessage(name_buffer);
	}
	free((void*)name_buffer);
	return ret;
}

static inline bool GetPatchTestValue(const char *const name, size_t name_length) {
	const char *const name_buffer = strndup(name, name_length);
	PatchTestNotExistWarningMessage(name_buffer);
	free((void*)name_buffer);
	return 1;
}

static inline bool GetCPUFeatureTest(const char *const name, size_t name_length) {
	const char *const name_buffer = strndup(name, name_length);
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

static inline size_t GetCodecaveAddress(const char *const name, size_t name_length, bool is_relative, x86_reg_t *const regs, size_t rel_source) {
	char *const name_buffer = strndup(name, name_length);
	char* user_offset_expr = strchr(name_buffer, '+');
	if (user_offset_expr) {
		// This prevents next read to name_buffer
		// from containing the offset value
		*user_offset_expr++ = '\0';
	}
	size_t ret = func_get(name_buffer);
	if (!ret) {
		CodecaveNotFoundWarningMessage(name_buffer);
	} else {
		if (user_offset_expr) {
			size_t user_offset_value;
			if (user_offset_expr == eval_expr_new_impl(user_offset_expr, user_offset_value, '\0', regs, rel_source)) {
				ExpressionErrorMessage();
			} else {
				ret += user_offset_value;
			}
		}
		if (is_relative) {
			ret -= rel_source + sizeof(void*);
		}
	}
	free(name_buffer);
	return ret;
}

static inline size_t GetBPFuncOrRawAddress(const char *const name, size_t name_length, bool is_relative, x86_reg_t *const regs, size_t rel_source) {
	const char *const name_buffer = strndup(name, name_length);
	size_t ret = func_get(name_buffer);
	if (!ret) { // Will be null if the name was not a BP function
		if (name_buffer == eval_expr_new_impl(name_buffer, ret, is_relative ? ']' : '>', regs, rel_source)) {
			ExpressionErrorMessage();
			is_relative = false;
			// Ret would still be 0, so keep going
			// to free the name string and return 0
		}
	}
	if (is_relative) {
		ret -= rel_source + sizeof(void*);
	}
	free((void*)name_buffer);
	return ret;
}

static inline const char* get_patch_value(const char* expr, size_t &out, x86_reg_t *const regs, size_t rel_source) {
	const char opening = expr[0];
	log_printf("Patch value opening char: \"%hhX\"\n", opening);
	bool is_relative = opening == '[';
	const char *const patch_val_end = find_matching_end(expr, opening, is_relative ? ']' : '>');
	if (!patch_val_end) {
		//Bracket error
		return expr;
	}
	// Increment expr so that the comparisons
	// don't check the openiong bracket
	++expr;
	// This would be a lot simpler if there were a variant of
	// patch_opt_get and func_get that took a length parameter
	if (strnicmp(expr, "codecave:", 9) == 0) {
		out = GetCodecaveAddress(expr, PtrDiffStrlen(patch_val_end, expr), is_relative, regs, rel_source);
	} else if (strnicmp(expr, "option:", 7) == 0) {
		expr += 7;
		out = GetOptionValue(expr, PtrDiffStrlen(patch_val_end, expr));
	} else if (strnicmp(expr, "patch:", 6) == 0) {
		expr += 6;
		out = GetPatchTestValue(expr, PtrDiffStrlen(patch_val_end, expr));
	} else if (strnicmp(expr, "cpuid:", 6) == 0) {
		expr += 6;
		out = GetCPUFeatureTest(expr, PtrDiffStrlen(patch_val_end, expr));
	} else {
		out = GetBPFuncOrRawAddress(expr, PtrDiffStrlen(patch_val_end, expr), is_relative, regs, rel_source);
	}
	return patch_val_end + 1;
};

static inline const char* CheckCastType(const char* expr, uint8_t &out) {
	if (expr[0] && expr[1] && expr[2]) {
		uint32_t cast = *(uint32_t*)expr;
		log_printf("Cast debug: %X\n");
		if (cast == ConstExprTextInt('I', '3', '2', ')') ||
			cast == ConstExprTextInt('i', '3', '2', ')')) {
				out = i32_cast;
				return expr + 4;
		}
		if (cast == ConstExprTextInt('U', '3', '2', ')') ||
			cast == ConstExprTextInt('u', '3', '2', ')')) {
				out = u32_cast;
				return expr + 4;
		}
		if (cast == ConstExprTextInt('F', '3', '2', ')') ||
			cast == ConstExprTextInt('f', '3', '2', ')')) {
				out = f32_cast;
				return expr + 4;
		}
		if (cast == ConstExprTextInt('I', '1', '6', ')') ||
			cast == ConstExprTextInt('i', '3', '2', ')')) {
				out = i16_cast;
				return expr + 4;
		}
		if (cast == ConstExprTextInt('U', '1', '6', ')') ||
			cast == ConstExprTextInt('u', '1', '6', ')')) {
				out = u16_cast;
				return expr + 3;
		}
		cast &= 0x00FFFFFF;
		if (cast == ConstExprTextInt('I', '8', ')') ||
			cast == ConstExprTextInt('i', '8', ')')) {
				out = i8_cast;
				return expr + 3;
		}
		if (cast == ConstExprTextInt('U', '8', ')') ||
			cast == ConstExprTextInt('u', '8', ')')) {
				out = u8_cast;
				return expr + 3;
		}
	}
	return expr;
};

// Were these even used originally?
static inline const char* CheckPointerSize(const char *const expr, uint8_t &out) {
	switch (expr[0]) {
		case 'b': case 'B':
			if (strnicmp(expr, "byte ptr", 8) == 0) {
				out = 1;
				return expr + 8;
			}
			break;
		case 'w': case 'W':
			if (strnicmp(expr, "word ptr", 8) == 0) {
				out = 2;
				return expr + 8;
			}
			break;
		case 'd': case 'D':
			if (strnicmp(expr, "dword ptr", 9) == 0) {
				out = 4;
				return expr + 9;
			}
			break;
		case 'f': case 'F':
			if (strnicmp(expr, "float ptr", 9) == 0) {
				out = 4;
				return expr + 9;
			}
			break;
	}
	out = 4;
	return expr;
};

static inline bool is_reg_name(const char *const expr, size_t &out, x86_reg_t *const regs) {
	if (!expr[0] || !expr[1] || !expr[2]) {
		return false;
	}
	// Since !expr_ref[2] already validated the string has at least
	// three non-null characters, there must also be at least one
	// more character because of the null terminator, thus it's fine
	// to just read a whole 4 bytes at once.
	uint32_t cmp = *(uint32_t*)expr & 0x00FFFFFF;
	strlwr((char*)&cmp);
	switch (cmp) {
		default:
			return false;
		case ConstExprTextInt('e', 'a', 'x'): out = regs->eax; break;
		case ConstExprTextInt('e', 'c', 'x'): out = regs->ecx; break;
		case ConstExprTextInt('e', 'd', 'x'): out = regs->edx; break;
		case ConstExprTextInt('e', 'b', 'x'): out = regs->ebx; break;
		case ConstExprTextInt('e', 's', 'p'): out = regs->esp; break;
		case ConstExprTextInt('e', 'b', 'p'): out = regs->ebp; break;
		case ConstExprTextInt('e', 's', 'i'): out = regs->esi; break;
		case ConstExprTextInt('e', 'd', 'i'): out = regs->edi; break;
	}
	log_printf("Found register \"%s\"\n", (char*)&cmp);
	return true;
};

static inline const char* consume_value_impl(const char* expr, size_t &out, char end, x86_reg_t *const regs, size_t rel_source) {
	// Current is used as both "current_char" and "cur_value"
	uint32_t current = 0;
	// cast is used for both casts and pointer sizes
	uint8_t cast = u32_cast;
	const char* expr_next;
	--expr;
	while (expr = CheckPointerSize(expr, cast), current = *(unsigned char*)++expr) {
		switch (current) {
			/*
			*  Skip whitespace
			*/
			case ' ': case '\t': case '\v': case '\f':
				continue;
			/*
			*  Unary Operators
			*/
			case '!': case '~': case '+': case '-': //case '*': case '&':
				// expr + 1 is used to avoid creating a loop
				expr_next = consume_value_impl(expr + 1, current, end, regs, rel_source);
				if (expr + 1 == expr_next) {
					goto InvalidValueError;
				}
				switch (expr[0]) {
					case '!': current = !current; break;
					case '~': current = ~current; break;
					//case '*': current = *(size_t*)current; break;
					//case '&': current = (size_t)&current; break;
					case '-': expr[1] == '-' ? --current : current *= -1; break;
					case '+': expr[1] == '+' ? ++current : current; break;
				}
				goto PostfixCheck;
			/*
			*  Casts and subexpression values
			*/
			case '(':
				expr_next = CheckCastType(expr + 1, cast);
				if (expr + 1 != expr_next) {
					log_printf("Cast success\n");
					// Casts
					expr = expr_next;
					expr_next = consume_value_impl(expr, current, end, regs, rel_source);
					if (expr == expr_next) {
						goto InvalidValueError;
					}
					switch (cast) {
						case i8_cast: current = *(int8_t*)&current; break;
						case i16_cast: current = *(int16_t*)&current; break;
						case i32_cast: current = *(int32_t*)&current; break;
						case u8_cast: current = *(uint8_t*)&current; break;
						case u16_cast: current = *(uint16_t*)&current; break;
						case u32_cast: current = *(uint32_t*)&current; break;
						case f32_cast: current = *(float*)&current; break;
					}
				} else {
					// Subexpressions
					// expr + 1 is used to avoid creating a loop
					expr_next = eval_expr_new_impl(expr + 1, current, ')', regs, rel_source);
					if (expr + 1 == expr_next) {
						goto InvalidExpressionError;
					}
					if (is_binhack) {
						--expr_next;
					}
				}
				goto PostfixCheck;
			/*
			*  Patch value and/or dereference
			*/
			case '[':
				if (is_breakpoint) {
			/*
			*  Dereference
			*/
			case '{':
					// expr + 1 is used to avoid creating a loop
					expr_next = eval_expr_new_impl(expr + 1, current, current == '[' ? ']' : '}', regs, rel_source);
					if (expr + 1 == expr_next) {
						goto InvalidExpressionError;
					}
					memcpy(&current, reinterpret_cast<void *>(current), cast);
					goto PostfixCheck;
				}
				[[fallthrough]];
			/*
			*  Guaranteed patch value
			*/
			case '<':
				// DON'T use expr + 1 since that kills get_patch_value
				expr_next = get_patch_value(expr, current, regs, rel_source);
				if (expr == expr_next) {
					goto InvalidPatchValueError;
				}
				// If the previous character was the end character,
				// back up so that the main function will be able
				// to detect it and not loop. Yes, it's a bit jank.
				if (expr_next[-1] == end) {
					--expr_next;
				}
				log_printf("Parsed patch value is %X / %d / %u\n", current, current, current);
				goto PostfixCheck;
			/*
			*  Raw value or breakpoint register
			*/
			default:
				if (is_breakpoint && is_reg_name(expr, current, regs)) {
					// current is set inside is_reg_name
					// TODO : Make less dependent on register names being 3 letters
					expr += 3;
					log_printf("Register value was %X / %d / %u\n", current, current, current);
				} else {
					// TODO : Don't parse raw hex with the address function
					str_address_ret_t addr_ret;
					current = str_address_value(expr, nullptr, &addr_ret);
					if (expr == addr_ret.endptr || (addr_ret.error && addr_ret.error != STR_ADDRESS_ERROR_GARBAGE)) {
						goto InvalidCharacterError;
					}
					log_printf("Raw value was %X / %d / %u\n", current, current, current);
					expr_next = addr_ret.endptr;
					log_printf("Remaining after raw value: \"%s\"\n", expr_next);
				}
				goto PostfixCheck;
		}
	}
PostfixCheck:
	if (expr_next[0] == '+' && expr_next[1] == '+') {
		PostIncDecWarningMessage();
		expr_next += 2;
	} else if (expr_next[0] == '-' && expr_next[1] == '-') {
		PostIncDecWarningMessage();
		expr_next += 2;
	}
	out = current;
	return expr_next;

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

static inline const char* eval_expr_new_impl(const char* expr, size_t &out, char end, x86_reg_t *const regs, size_t rel_source) {

	enum ParsingMode : bool {
		Value,
		Operator
	};

	log_printf("START SUBEXPRESSION: \"%s\"\n", expr);

	size_t value = 0;
	op_t cur_op = NoOp;
	op_t next_op;
	ParsingMode mode = Value;
	const char* expr_next;
	size_t cur_value;
	log_printf("Current end character: \"%hhX\"\n", end);
	while (*expr && *expr != end) {
		log_printf("Remaining expression: \"%s\"\n", expr);
		if (mode == Value) {

			expr_next = consume_value_impl(expr, cur_value, end, regs, rel_source);
			if (expr == expr_next) goto InvalidValueError;
			expr = expr_next;

			value = ApplyOperator(cur_op, value, cur_value);
			log_printf("Result of operator was %X / %d / %u\n", value, value, value);
			mode = Operator;

		} else {

			expr_next = find_next_op_impl(expr, next_op);
			if (expr == expr_next || next_op == BadBrackets) goto InvalidCharacterError;
			expr = expr_next;

			PrintOp(next_op);
			if (CompareOpPrecedence(cur_op, next_op) > 0) {
				// Encountering an operator with a higher precedence can
				// be solved by recursing eval_expr over the remaingng text
				expr_next = eval_expr_new_impl(expr, cur_value, end, regs, rel_source);
				if (expr == expr_next) goto InvalidExpressionError;
				expr = expr_next;
			}
			cur_op = next_op;
			mode = Value;
		}
	}
	log_printf("END SUBEXPRESSION\nSubexpression value was %X / %d / %u\n", value, value, value);
	out = value;
	return expr;

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

const char* eval_expr(const char* expr, size_t* out, char end, x86_reg_t* regs, size_t rel_source) {
	log_printf("START EXPRESSION \"%s\" with end \"%hhX\"\n", expr, end);
	const char *const expr_next = eval_expr_new_impl(expr, *out, end, regs, rel_source);
	if (expr == expr_next) {
		ExpressionErrorMessage();
		*out = 0;
	}
	log_printf("END EXPRESSION\nFinal result was: %X / %d / %u\nRemaining after final: \"%s\"\n", *out, *out, *out, expr_next);
	return expr_next;
}
