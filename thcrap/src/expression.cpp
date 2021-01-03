/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Expression parsing
  */

#include "thcrap.h"
constexpr uint32_t ConstExprTextInt(unsigned char one, unsigned char two = '\0', unsigned char three = '\0', unsigned char four = '\0') {
	return four << 24 | three << 16 | two << 8 | one;
}

static inline char* strndup(const char* source, size_t length) {
	char *const ret = (char *const)malloc(length+1);
	strncpy(ret, source, length);
	ret[length] = '\0';
	return ret;
}

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
	Increment = '+' + '+',
	Decrement = '-' + '-',
	LogicalNot = PreventOverlap('!'),
	BitwiseNot = '~',
	Multiply = '*',
	Divide = '/',
	Modulo = '%',
	Add = '+',
	Subtract = '-',
	LogicalLeftShift = '<' + '<',
	LogicalRightShift = '>' + '>',
	ArithmeticLeftShift = '<' + '<' + '<',
	ArithmeticRightShift = '>' + '>' + '>',
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
	TernaryStart = '?',
	TernaryEnd = PreventOverlap(':'),
	NullCoalescing = '?' + PreventOverlap(':'),
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

inline const char* parse_brackets(const char* str, char opening) {
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

static op_t __fastcall find_next_op(const char * *const expr, const char * *const pre_op_ptr) {
	uint32_t c;
	const char* expr_ref = *expr - 1;
	for (;;) {
		switch (c = *(unsigned char*)++expr_ref) {
			case '\0':
				return c;
			case '(': case '[': /*case '{':*/ {
				const char* end_bracket = parse_brackets(expr_ref, c);
				if (*end_bracket == c) {
					return BadBrackets;
				}
				expr_ref = end_bracket + 1;
				continue;
			}
			case ':':
				*pre_op_ptr = expr_ref;
				*expr = &expr_ref[1];
				return c * 2;
			case '&': case '|': case '!':
				c += c;
			case '<': case '>':
			case '+': case '-': case '*': case '/': case '%': case '=': case '^': case '?':
				uint32_t temp;
				if (expr_ref[1]) {
					if ((c == '<' || c == '>') && expr_ref[2]) {
						temp = *(uint32_t*)expr_ref;
						// Yeah, the if chain is kind of ugly. However, MSVC doesn't
						// optimize this into a jump table and using a switch adds an
						// extra check for "higher than included" values, which are
						// impossible anyway since they already got filtered out.
						if (temp == ConstExprTextInt('>', '>', '>', '=') ||
							temp == ConstExprTextInt('<', '<', '<', '=')) {
								*pre_op_ptr = expr_ref;
								*expr = &expr_ref[4];
								return c * 3 + '=';
						}
						if (temp == ConstExprTextInt('>', '>', '=') ||
							temp == ConstExprTextInt('<', '<', '=')) {
								*pre_op_ptr = expr_ref;
								*expr = &expr_ref[3];
								return c * 2 + '=';
						}
						if (temp == ConstExprTextInt('>', '>', '>') ||
							temp == ConstExprTextInt('<', '<', '<')) {
								*pre_op_ptr = expr_ref;
								*expr = &expr_ref[3];
								return c * 3;
						}
					} else {
						temp = *(uint16_t*)expr_ref;
					}
					if (temp == ConstExprTextInt('>', '>') || temp == ConstExprTextInt('<', '<') ||
						temp == ConstExprTextInt('+', '+') || temp == ConstExprTextInt('-', '-') ||
						temp == ConstExprTextInt('&', '&') || temp == ConstExprTextInt('|', '|')) {
							*pre_op_ptr = expr_ref;
							*expr = &expr_ref[2];
							return c * 2;
					}
					if (temp == ConstExprTextInt('|', '=') || temp == ConstExprTextInt('^', '=') ||
						temp == ConstExprTextInt('!', '=') || temp == ConstExprTextInt('%', '=') ||
						temp == ConstExprTextInt('&', '=') || temp == ConstExprTextInt('*', '=') ||
						temp == ConstExprTextInt('+', '=') || temp == ConstExprTextInt('-', '=') ||
						temp == ConstExprTextInt('/', '=')) {
							*pre_op_ptr = expr_ref;
							*expr = &expr_ref[2];
							return c + '=';
					}
					if (temp == ConstExprTextInt('?', ':')) {
							*pre_op_ptr = expr_ref;
							*expr = &expr_ref[2];
							return NullCoalescing;
					}
				}
			case ',': case '~':
				*pre_op_ptr = expr_ref;
				*expr = &expr_ref[1];
				return c;
		}
	}
}

static inline unsigned char OpPrecedence(op_t op) {
	switch (op) {
		case NoOp:
			return 15;
		case Increment: case Decrement: case LogicalNot: case BitwiseNot:
			return 14;
		case Multiply: case Divide: case Modulo:
			return 13;
		case Add: case Subtract:
			return 12;
		case LogicalLeftShift: case LogicalRightShift: case ArithmeticLeftShift: case ArithmeticRightShift:
			return 11;
		case Less: case LessEqual: case Greater: case GreaterEqual:
			return 10;
		case Equal: case NotEqual:
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
		case TernaryStart: case NullCoalescing:
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
		case NoOp: op_text = "NoOp"; break;
		case Add: op_text = "+"; break;
		case Subtract: op_text = "-"; break;
		case Multiply: op_text = "*"; break;
		case Divide: op_text = "/"; break;
		case Modulo: op_text = "%"; break;
		case LogicalLeftShift: op_text = "<<"; break;
		case LogicalRightShift: op_text = ">>"; break;
		case ArithmeticLeftShift: op_text = "<<<"; break;
		case ArithmeticRightShift: op_text = ">>>"; break;
		case BitwiseAnd: op_text = "&"; break;
		case BitwiseXor: op_text = "^"; break;
		case BitwiseOr: op_text = "|"; break;
		case BitwiseNot: op_text = "~"; break;
		case LogicalAnd: op_text = "&&"; break;
		case LogicalOr: op_text = "||"; break;
		case LogicalNot: op_text = "!"; break;
		case Less: op_text = "<"; break;
		case LessEqual: op_text = "<="; break;
		case Greater: op_text = ">"; break;
		case GreaterEqual: op_text = ">="; break;
		case Equal: op_text = "=="; break;
		case NotEqual: op_text = "!="; break;
		case Comma: op_text = ","; break;
		case Increment: op_text = "++"; break;
		case Decrement: op_text = "--"; break;
		case TernaryStart: op_text = "?"; break;
		case TernaryEnd: op_text = ":"; break;
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
		default: op_text = "ERROR"; break;
	}
	log_printf("Found operator \"%s\"\n", op_text);
}

static inline char CompareOpPrecedence(op_t PrevOp, op_t NextOp) {
	// TODO: Change to <=> once C++ is updated
	const char NextPrecedence = OpPrecedence(NextOp);
	const char PrevPrecedence = OpPrecedence(PrevOp);
	char ret = 0;
	ret += NextPrecedence > PrevPrecedence;
	ret -= NextPrecedence < PrevPrecedence;
	return ret;
}

#define WarnOnce(warning) do {\
	static bool AlreadyDisplayedWarning = false;\
	if (!AlreadyDisplayedWarning) { \
		warning; \
		AlreadyDisplayedWarning = true; \
	}\
} while (0)

static inline void IncDecWarning(void) {
	WarnOnce(log_printf("EXPRESSION WARNING 0: Increment and decrement operators do not currently function as expected because it is not possible to modify the value of an option in an expression. These operators are only included for future compatibility and operator precedence reasons.\n"));
}

static inline void AssignmentWarning(void) {
	WarnOnce(log_printf("EXPRESSION WARNING 1: Assignment operators do not currently function as expected because it is not possible to modify the value of an option in an expression. These operators are only included for future compatibility and operator precedence reasons.\n"));
}

static inline void TernaryWarning(void) {
	WarnOnce(log_printf("EXPRESSION WARNING 2: Expressions currently don't support ternary operations with correct associativity. However, it is still possible to obtain a correct result with parenthesis.\n"));
}

static inline void PatchValueWarning(const char *const name) {
	log_printf("EXPRESSION WARNING 3: Unknown patch value type \"%s\", using value 0\n", name);
}

static inline void CodecaveNotFoundError(const char *const name) {
	log_printf("EXPRESSION WARNING 4: Codecave \"%s\" not found\n", name);
}

static inline void PatchTestNotExistError(const char *const name) {
	log_printf("EXPRESSION WARNING 5: Couldn't test for patch \"%s\" because patch testing hasn't been implemented yet, assuming it exists and returning 1...");
}

static inline size_t GroupingBracketError(void) {
	log_printf("EXPRESSION ERROR 0: Unmatched grouping brackets\n");
	return 0;
}

static inline size_t ValueBracketError(void) {
	log_printf("EXPRESSION ERROR 1: Unmatched patch value brackets\n");
	return 0;
}

static inline size_t BadCharacterError(void) {
	log_printf("EXPRESSION ERROR 2: Unknown character\n");
	return 0;
}

static inline size_t OptionNotFoundError(const char *const name) {
	log_printf("EXPRESSION ERROR 3: Option \"%s\" not found\n", name);
	return 0;
}

static inline size_t ApplyOperator(op_t op, size_t value, size_t arg) {
	switch (op) {
		case Increment:
			IncDecWarning();
			return value + 1;
		case Decrement:
			IncDecWarning();
			return value - 1;
		case LogicalNot:
			return !value;
		case BitwiseNot:
			return ~value;
		case MultiplyAssign:
			AssignmentWarning();
		case Multiply:
			return value * arg;
		case DivideAssign:
			AssignmentWarning();
		case Divide:
			return value / arg;
		case ModuloAssign:
			AssignmentWarning();
		case Modulo:
			return value % arg;
		case AddAssign:
			AssignmentWarning();
		case Add:
			return value + arg;
		case SubtractAssign:
			AssignmentWarning();
		case Subtract:
			return value - arg;
		case LogicalLeftShiftAssign:
		case ArithmeticLeftShiftAssign:
			AssignmentWarning();
		case LogicalLeftShift:
		case ArithmeticLeftShift:
			return value << arg;
		case LogicalRightShiftAssign:
			AssignmentWarning();
		case LogicalRightShift:
			return value >> arg;
		case ArithmeticRightShiftAssign:
			AssignmentWarning();
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
		case TernaryStart:
			TernaryWarning();
			return value;
			break;
		case NullCoalescing:
			TernaryWarning();
			return value ? value : arg;
			break;
		case Assign:
			AssignmentWarning();
		case Comma:
			//why tho
		case NoOp:
		default:
			return arg;
	}
}

static inline bool consume(const char * *const str, const stringref_t token) {
	if (!!strnicmp(*str, token.str, token.len)) return false;
	*str += token.len;
	return true;
}

static size_t eval_expr_new_impl(const char **expr_ptr, x86_reg_t *regs, char end, size_t rel_source) {

	enum ParsingMode : bool {
		Value,
		Operator
	};

	enum CastType : uint8_t {
		invalid_cast,
		u8_cast,
		u16_cast,
		u32_cast,
		i8_cast,
		i16_cast,
		i32_cast,
		f32_cast
	};

	const char *expr = *expr_ptr;

	log_printf("START SUBEXPRESSION: \"%s\"\n", expr);

	size_t value = 0;
	op_t prev_op = NoOp;
	op_t next_op = NoOp;
	CastType current_cast = u32_cast;
	ParsingMode mode = Value;
#define breakpoint_test_var regs
#define is_breakpoint (breakpoint_test_var)
#define is_binhack (!breakpoint_test_var)
#define is_bracket(c) (c == '[')

	/// Parser functions
		/// ----------------

#define consume_whitespace() for (; current_char == ' ' || current_char == '\t'; current_char = *++expr)

	auto is_reg_name = [regs](const char * *const expr, size_t* value_out) {
		if (!regs) {
			return false;
		}
		const char *const expr_ref = *expr;
		if (!expr_ref[0] || !expr_ref[1] || !expr_ref[2]) {
			return false;
		}
		// Since !expr_ref[2] already validated the string has at least
		// three non-null characters, there must also be at least one
		// more character because of the null terminator, thus it's fine
		// to just read a whole 4 bytes at once.
		uint32_t cmp = *(uint32_t*)expr_ref & 0x00FFFFFF;
		strlwr((char*)&cmp);
		switch (cmp) {
			default:
				return false;
			case ConstExprTextInt('e', 'a', 'x'): *value_out = regs->eax; break;
			case ConstExprTextInt('e', 'c', 'x'): *value_out = regs->ecx; break;
			case ConstExprTextInt('e', 'd', 'x'): *value_out = regs->edx; break;
			case ConstExprTextInt('e', 'b', 'x'): *value_out = regs->ebx; break;
			case ConstExprTextInt('e', 's', 'p'): *value_out = regs->esp; break;
			case ConstExprTextInt('e', 'b', 'p'): *value_out = regs->ebp; break;
			case ConstExprTextInt('e', 's', 'i'): *value_out = regs->esi; break;
			case ConstExprTextInt('e', 'd', 'i'): *value_out = regs->edi; break;
		}
		*expr = &expr_ref[3];
		log_printf("Found register \"%s\"\n", (char*)&cmp);
		return true;
	};

	auto get_patch_value = [rel_source, regs](const char * *const expr, char opening, size_t* out) {
		log_printf("Patch value opening char: \"%hhX\"\n", opening);
		bool is_relative = opening == '[';
		const char * expr_ref = *expr;
		const char *const patch_val_end = find_matching_end(expr_ref, opening, is_relative ? ']' : '>');
		if (!patch_val_end) {
			return false;
		}
		++expr_ref;
		size_t ret = 0;
		char * name_buffer = NULL;
		bool success_state = true;
		if (strnicmp(expr_ref, "codecave:", 9) == 0) {
			name_buffer = strndup(expr_ref, PtrDiffStrlen(patch_val_end, expr_ref));
			char* user_offset = strchr(name_buffer, '+');
			size_t user_offset_value = 0;
			if (user_offset) {
				*user_offset++ = '\0';
				user_offset_value = eval_expr_new_impl((const char**)&user_offset, regs, '\0', rel_source);
			}
			ret = func_get(name_buffer);
			if (!ret) {
				CodecaveNotFoundError(name_buffer);
				ret = 0;
				//success_state = false;
			} else {
				ret += user_offset_value;
			}
		} else if (consume(&expr_ref, "option:")) {
			is_relative = false;
			name_buffer = strndup(expr_ref, PtrDiffStrlen(patch_val_end, expr_ref));
			patch_opt_val_t *const option = patch_opt_get(name_buffer);
			if (option) {
				memcpy(&ret, option->val.byte_array, option->size);
			} else {
				ret = OptionNotFoundError(name_buffer);
				success_state = false;
			}
		} else if (consume(&expr_ref, "patch:")) {
			is_relative = false;
			name_buffer = strndup(expr_ref, PtrDiffStrlen(patch_val_end, expr_ref));
			PatchTestNotExistError(name_buffer);
			ret = 1;
		} else {
			name_buffer = strndup(expr_ref, PtrDiffStrlen(patch_val_end, expr_ref));
			ret = (size_t)func_get(name_buffer);
			if (!ret) {
				ret = eval_expr_new_impl(&expr_ref, regs, is_relative ? ']' : '>', rel_source);
			}
		}
		free(name_buffer);
		if (is_relative) {
			ret -= rel_source + sizeof(void*);
		}
		*out = ret;
		*expr = patch_val_end - 1;
		return success_state;
	};

	char current_char;

	auto CheckPointerSize = [current_char, &expr](void)->uint8_t {
		switch (current_char) {
			case 'b': case 'B':
				return consume(&expr, "byte ptr") ? 1 : 4;
			case 'w': case 'W':
				return consume(&expr, "word ptr") ? 2 : 4;
			case 'd': case 'D':
				return consume(&expr, "dword ptr") ? 4 : 4;
			case 'f': case 'F':
				return consume(&expr, "float ptr") ? 4 : 4;
		}
		return 4;
	};

	auto CheckCastType = [&expr](void)->CastType {
		if (expr[0] && expr[1] && expr[2]) {
			uint32_t cast = *(uint32_t*)expr;
			if (cast == ConstExprTextInt('I', '3', '2', ')') ||
				cast == ConstExprTextInt('i', '3', '2', ')')) {
					expr += 3;
					return i32_cast;
			}
			if (cast == ConstExprTextInt('U', '3', '2', ')') ||
				cast == ConstExprTextInt('u', '3', '2', ')')) {
					expr += 3;
					return u32_cast;
			}
			if (cast == ConstExprTextInt('F', '3', '2', ')') ||
				cast == ConstExprTextInt('f', '3', '2', ')')) {
					expr += 3;
					return f32_cast;
			}
			if (cast == ConstExprTextInt('I', '1', '6', ')') ||
				cast == ConstExprTextInt('i', '3', '2', ')')) {
					expr += 3;
					return i16_cast;
			}
			if (cast == ConstExprTextInt('U', '1', '6', ')') ||
				cast == ConstExprTextInt('u', '1', '6', ')')) {
					expr += 3;
					return u16_cast;
			}
			cast &= 0x00FFFFFF;
			if (cast == ConstExprTextInt('I', '8', ')') ||
				cast == ConstExprTextInt('i', '8', ')')) {
					expr += 2;
					return i8_cast;
			}
			if (cast == ConstExprTextInt('U', '8', ')') ||
				cast == ConstExprTextInt('u', '8', ')')) {
					expr += 2;
					return u8_cast;
			}
		}
		return invalid_cast;
	};

	/// ----------------
	size_t cur_value;
	log_printf("Current end character: \"%hhX\"\n", end);
	for (current_char = *expr; current_char && current_char != end; current_char = *++expr) {
		consume_whitespace();
		log_printf("Remaining expression: \"%s\"\n", expr);
		switch (mode) {
			case Value: {
				bool is_negative = current_char == '-';
				expr += is_negative;
				current_char = *expr;
				unsigned char ptr_size = sizeof(int32_t);
				switch (current_char) {
					case '(': {
						++expr;
						CastType temp = CheckCastType();
						if (temp != invalid_cast) {
							current_cast = temp;
							continue;
						} else {
							cur_value = eval_expr_new_impl(&expr, regs, ')', rel_source);
						}
						break;
					}
					case '[':
						if (is_breakpoint) {
							++expr;
							if (cur_value = eval_expr_new_impl(&expr, regs, ']', rel_source)) {
								size_t ret = 0;
								memcpy(&ret, reinterpret_cast<void *>(cur_value), ptr_size);
								cur_value = ret;
							}
							break;
						}
					case '<':
						if (!get_patch_value(&expr, current_char, &cur_value)) {
							*expr_ptr = expr;
							return ValueBracketError();
						}
						if (*(expr + 1) != end) ++expr;
						log_printf("Parsed patch value is %X / %d / %u\n", cur_value, cur_value, cur_value);
						break;
					case '{':
					default:
						if (is_breakpoint && is_reg_name(&expr, &cur_value)) {
							//cur_value is set inside is_reg_name
							log_printf("Register value was %X / %d / %u\n", cur_value, cur_value, cur_value);
						} else {
							str_address_ret_t addr_ret;
							cur_value = str_address_value(expr, nullptr, &addr_ret);
							if (expr == addr_ret.endptr || (addr_ret.error && addr_ret.error != STR_ADDRESS_ERROR_GARBAGE)) {
								*expr_ptr = expr;
								return BadCharacterError();
							}
							log_printf("Raw value was %X / %d / %u\n", cur_value, cur_value, cur_value);
							expr = addr_ret.endptr - 1;
							log_printf("Remaining after raw value: \"%s\"\n", addr_ret.endptr);
						}
				}
				cur_value = is_negative ? cur_value * -1 : cur_value;
				log_printf("Value was %X / %d / %u\n", cur_value, cur_value, cur_value);
				value = ApplyOperator(prev_op, value, cur_value);
				log_printf("Result of operator was %X / %d / %u\n", value, value, value);
				mode = Operator;
				break;
			}
			case Operator: {
				const char * op_ptr = expr;
				const char * pre_op_ptr;
				next_op = find_next_op(&op_ptr, &pre_op_ptr);
				if (next_op == BadBrackets) {
					*expr_ptr = expr;
					return ValueBracketError();
				}
				PrintOp(next_op);
				expr = op_ptr - 1;
				if (CompareOpPrecedence(prev_op, next_op) > 0) {
					cur_value = eval_expr_new_impl(&expr, regs, end, rel_source);
				}
				prev_op = next_op;
				mode = Value;
				break;
			}
		}
	}
	log_printf("END SUBEXPRESSION\n");
	log_printf("Subexpression value was %X / %d / %u\n", value, value, value);
	*expr_ptr = expr;
	return value;
}

size_t eval_expr(const char **expr_ptr, x86_reg_t *regs, char end, size_t rel_source) {
	log_printf("START EXPRESSION with end \"%hhX\"\n", end);
	const size_t ret = eval_expr_new_impl(expr_ptr, regs, end, rel_source);
	log_printf("END EXPRESSION\nFinal result was: %X / %d / %u\n", ret, ret, ret);
	return ret;
}
