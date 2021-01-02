/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Expression parsing
  */

#include "thcrap.h"

static inline char* strndup(const char* source, size_t length) {
	char *const ret = (char *const)malloc(length);
	return (char*)memcpy(ret, source, length);
}

#define PtrDiffStrlen(end_ptr, start_ptr) ((end_ptr) - (start_ptr))

#define EAX 0x00786165
#define ECX 0x00786365
#define EDX 0x00786465
#define EBX 0x00786265
#define ESP 0x00707365
#define EBP 0x00706265
#define ESI 0x00697365
#define EDI 0x00696465

size_t* reg(x86_reg_t *regs, const char *regname, const char **endptr)
{
	uint32_t cmp;

	if(!regs || !regname || !regname[0] || !regname[1] || !regname[2]) {
		return NULL;
	}
	memcpy(&cmp, regname, 3);
	cmp &= 0x00FFFFFF;
	strlwr((char *)&cmp);

	if(endptr) {
		*endptr = regname + 3;
	}
	switch(cmp) {
		case EAX: return &regs->eax;
		case ECX: return &regs->ecx;
		case EDX: return &regs->edx;
		case EBX: return &regs->ebx;
		case ESP: return &regs->esp;
		case EBP: return &regs->ebp;
		case ESI: return &regs->esi;
		case EDI: return &regs->edi;
	}
	if(endptr) {
		*endptr = regname;
	}
	return NULL;
}

//bool new_reg(size_t *const value, x86_reg_t *const regs, const char * *const expr) {
//	if (!regs || !value || !expr) {
//		return false;
//	}
//	const char *const expr_ref = *expr;
//	if (!expr_ref[0] || !expr_ref[1] || !expr_ref[2]) {
//		return false;
//	}
//	// Since !expr_ref[2] already validated the string has at least
//	// three non-null characters, there must also be at least one
//	// more character because of the null terminator, thus it's fine
//	// to just read a whole 4 bytes at once.
//	uint32_t cmp = { *(uint32_t*)expr_ref & 0x00FFFFFF };
//	strlwr((char*)&cmp);
//	switch (cmp) {
//		default:
//			return false;
//		case EAX: *value = regs->eax; break;
//		case ECX: *value = regs->ecx; break;
//		case EDX: *value = regs->edx; break;
//		case EBX: *value = regs->ebx; break;
//		case ESP: *value = regs->esp; break;
//		case EBP: *value = regs->ebp; break;
//		case ESI: *value = regs->esi; break;
//		case EDI: *value = regs->edi; break;
//	}
//	*expr = &expr_ref[3];
//	return true;
//}

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

#define ARSA 0x3D3E3E3E
#define ALSA 0x3D3C3C3C
#define ARS  0x003E3E3E
#define ALS  0x003C3C3C
#define LRSA 0x003D3E3E
#define LLSA 0x003D3C3C
#define LRS  0x3E3E
#define LLS  0x3C3C
#define GEQ  0x3D3E
#define LEQ  0x3D3C
#define EQU  0x3D3D
#define NEQ  0x3D21
#define ADDA 0x3D2B
#define SUBA 0x3D2D
#define MULA 0x3D2A
#define DIVA 0x3D2F
#define MODA 0x3D25
#define ANDA 0x3D26
#define ORA  0x3D7C
#define XORA 0x3D5E
#define LAND 0x2626
#define LOR  0x7C7C
#define INC  0x2B2B
#define DEC  0x2D2D
#define NULC 0x3A3F

#define I32P 0x29323349
#define i32P 0x29323369
#define I16P 0x29363149
#define i16P 0x29363169
#define I8P 0x00293849
#define i8P 0x00293869
#define U32P 0x29323355
#define u32P 0x29323375
#define U16P 0x29363155
#define u16P 0x29363175
#define U8P 0x00293855
#define u8P 0x00293875
#define F32P 0x29323346
#define f32P 0x29323366

//typedef struct {
//	unsigned char byte_one;
//	unsigned char byte_two;
//	unsigned char byte_three;
//} ThreeBytes;
//typedef union {
//	ThreeBytes in;
//	struct {
//		unsigned char byte_one;
//		unsigned char byte_two;
//		unsigned char byte_three;
//		unsigned char byte_four;
//	};
//	uint32_t out;
//} ThreeBytesOrInt;
//static inline op_t find_next_op_long(const char * *const expr) {
////#define expr_ref (*expr)
//	uint32_t c;
//	const char* expr_ref = --(*expr);
//	unsigned char min_length = 1;
//repeat_label:
//	switch (c = *++expr_ref) {
//		default:
//			//++expr_ref;
//			goto repeat_label;
//		case '\0':
//			--expr_ref;
//			return c;
//			//goto End;
//		case '<': case '>':
//			//if (*(expr_ref + 1)) {
//			if (*expr_ref) {
//				++min_length;
//				//if (*(expr_ref + 2)) {
//				if (*(expr_ref + 1)) {
//					//min_length += 1 + !!*(expr_ref + 3);
//					min_length += 1 + !!*(expr_ref + 2);
//				}
//			}
//			break;
//		case '&': case '|': case '!':
//			c += c;
//		case '+': case '-': case '*': case '/': case '%': case '=': case '^': case '?':
//			//min_length += !!*(expr_ref + 1);
//			min_length += !!*expr_ref;
//			break;
//		case ':':
//			c += c;
//		case ',': case '~':
//			break;
//	}
//	switch (c) {
//		case '<': case '>':
//			switch (min_length) {
//				case 4: {
//					const int temp = *(int*)expr_ref;
//					if (temp == ARSA || temp == ALSA) {
//						//expr_ref += 4;
//						expr_ref += 3;
//						c *= 3;
//						c += '=';
//						goto End;
//					}
//				}
//				case 3: {
//					const ThreeBytesOrInt temp = { *(ThreeBytes*)expr_ref };
//					//expr_ref += 2;
//					//switch (temp.out) {
//					if (temp.out == LRSA || temp.out == LLSA) {
//						//case LRSA: case LLSA:
//							//expr_ref += 3;
//							expr_ref += 2;
//							//return c * 2 + '=';
//						c *= 2;
//						c += '=';
//						goto End;
//					} else if (temp.out == ARS || temp.out == ALS) {
//						//case ARS: case ALS:
//							//expr_ref += 3;
//							expr_ref += 2;
//							//return c * 3;
//						c *= 3;
//						goto End;
//					}
//					//}
//					//expr_ref -= 2;
//				}
//			}
//		case '=':
//		default:
//			if (min_length > 1) {
//				//++expr_ref;
//				switch (*(short*)expr_ref) {
//					case LRS: case LLS: case INC: case DEC: case EQU: case LAND: case LOR:
//						//expr_ref += 2;
//						++expr_ref;
//						//return c * 2;
//						c *= 2;
//						goto End;
//					
//						//expr_ref += 2;
//						//++expr_ref;
//						//return c * 4;
//						//c *= 4;
//						//goto End;
//					case NEQ: case ADDA: case SUBA: case MULA: case DIVA: case MODA: case XORA: case ANDA: case ORA:
//						//expr_ref += 2;
//						++expr_ref;
//						//return c + '=';
//						c += '=';
//						goto End;
//					
//						//expr_ref += 2;
//						//++expr_ref;
//						//return c * 2 + '=';
//						//c *= 2;
//						//c += '=';
//						//goto End;
//					case NULC:
//						//expr_ref += 2;
//						++expr_ref;
//						//return NullCoalescing;
//						c = NullCoalescing;
//						goto End;
//				}
//				//--expr_ref;
//			}
//			//++expr_ref;
//			//switch (c) {
//				//case '!': case '&': case '|': case ':':
//					//return (c << 1);
//					//c *= 2;
//					//goto End;
//				/*case '+': case '-': case '*': case '/': case '%': case '^':
//				case '~': case '=': case '?': case ',': case '<': case '>':
//					return c;*/
//				
//			//}
//	}
//End:
//	*expr = expr_ref;
//	return c;
////#undef expr_ref
//};
//static inline op_t find_next_op_last_good(const char * *const expr) {
//	uint32_t c;
//	const char* expr_ref = --(*expr);
//	unsigned char min_length = 1;
//repeat_label:
//	switch (c = *++expr_ref) {
//		default:
//			goto repeat_label;
//		case '\0':
//			--expr_ref;
//			return c;
//		case '<': case '>':
//			if (*expr_ref) {
//				++min_length;
//				if (*(expr_ref + 1)) {
//					min_length += 1 + !!*(expr_ref + 2);
//				}
//			}
//			break;
//		case '&': case '|': case '!':
//			c += c;
//		case '+': case '-': case '*': case '/': case '%': case '=': case '^': case '?':
//			min_length += !!*expr_ref;
//			break;
//		case ':':
//			c += c;
//		case ',': case '~':
//			break;
//	}
//	switch (c) {
//		case '<': case '>':
//			ThreeBytesOrInt temp;
//			switch (min_length) {
//				case 4:
//					//register const uint32_t temp = *(uint32_t*)expr_ref;
//					temp.out = *(uint32_t*)expr_ref;
//					if (temp.out == ARSA || temp.out == ALSA) {
//						expr_ref += 3;
//						c *= 3;
//						c += '=';
//						goto End;
//					} else {
//				case 3:
//					//register const CastThreeBytesToInt temp = { *(ThreeBytes*)expr_ref };
//					temp.in = *(ThreeBytes*)expr_ref;
//					}
//					temp.byte_four = 0;
//					if (temp.out == LRSA || temp.out == LLSA) {
//						expr_ref += 2;
//						c *= 2;
//						c += '=';
//						goto End;
//					} else if (temp.out == ARS || temp.out == ALS) {
//						expr_ref += 2;
//						c *= 3;
//						goto End;
//					}
//			}
//		case '=':
//		default:
//			if (min_length != 1) {
//				switch (*(uint16_t*)expr_ref) {
//					case LRS: case LLS: case INC: case DEC: case EQU: case LAND: case LOR:
//						++expr_ref;
//						c *= 2;
//						goto End;
//					case NEQ: case ADDA: case SUBA: case MULA: case DIVA: case MODA: case XORA: case ANDA: case ORA:
//						++expr_ref;
//						c += '=';
//						goto End;
//					case NULC:
//						++expr_ref;
//						c = NullCoalescing;
//						goto End;
//				}
//			}
//	}
//End:
//	*expr = expr_ref;
//	return c;
//};
//static inline op_t find_next_op_latest(const char * *const expr) {
//	uint32_t c;
//	const char* expr_ref = (*expr) - 1;
//	unsigned char min_length = 1;
//repeat_label:
//	++expr_ref;
//	switch (c = *expr_ref) {
//		default:
//			goto repeat_label;
//		case '\0':
//			goto EndZero;
//			//--expr_ref;
//			//return c;
//		case '<': case '>':
//			/*if (*(expr_ref + 1)) {
//				++min_length;
//				if (*(expr_ref + 2)) {
//					min_length += 1 + !!*(expr_ref + 3);
//				}
//			}*/
//			if (*(expr_ref + 1)) {
//				//if (*(expr_ref + 2)) {
//				//	//min_length = 3 + !!*(expr_ref + 3);
//				//	min_length = 3;
//				//	break;
//				//}
//				min_length = 2 + !!*(expr_ref + 2);
//				break;
//			}
//			break;
//		case '&': case '|': case '!':
//			c += c;
//		case '+': case '-': case '*': case '/': case '%': case '=': case '^': case '?':
//			min_length += !!*(expr_ref + 1);
//			break;
//		case ':':
//			c += c;
//		case ',': case '~':
//			break;
//	}
//	uint32_t temp = 0;
//	switch (min_length) {
//		if (0) {
//		case 4:
//		case 3:
//			temp = *(uint32_t*)expr_ref;
//			if (temp == ARSA || temp == ALSA) {
//				/*expr_ref += 3;
//				c *= 3;
//				c += '=';
//				goto End;*/
//				goto EndThreeEqual;
//			}
//			//temp &= 0x00FFFFFF;
//			//temp >> 8;
//			if (temp == LRSA || temp == LLSA) {
//				/*expr_ref += 2;
//				c *= 2;
//				c += '=';
//				goto End;*/
//				goto EndTwoEqual;
//			} else if (temp == ARS || temp == ALS) {
//				/*expr_ref += 2;
//				c *= 3;
//				goto End;*/
//				goto EndThree;
//			}
//			//temp &= 0x0000FFFF;
//			//temp >> 8;
//		} else {
//		case 2:
//			temp = *(uint16_t*)expr_ref;
//		}
//			if (temp == LRS || temp == LLS || temp == INC || temp == DEC || temp == EQU || temp == LAND || temp == LOR) {
//				goto EndTwo;
//			}
//			if (temp == ORA || temp == XORA || temp == NEQ || temp == MODA || temp == ANDA || temp == MULA || temp == ADDA || temp == SUBA || temp == DIVA) {
//				goto EndOneEqual;
//			}
//			if (temp == NULC) {
//				c = NullCoalescing;
//			}
//			//switch (temp) {
//			//	case LRS: case LLS: case INC: case DEC: case EQU: case LAND: case LOR:
//			//		/*++expr_ref;
//			//		c *= 2;
//			//		goto End;*/
//			//		goto EndTwo;
//			//	case NEQ: case ADDA: case SUBA: case MULA: case DIVA: case MODA: case XORA: case ANDA: case ORA:
//			//		/*++expr_ref;
//			//		c += '=';
//			//		goto End;*/
//			//		goto EndOneEqual;
//			//	case NULC:
//			//		/*++expr_ref;
//			//		c = NullCoalescing;
//			//		goto End;*/
//			//		//goto EndNC;
//			//		c = NullCoalescing;
//			//}
//	}
////EndOne:
//	*expr = expr_ref;
//EndZero:
//	return c;
//EndThreeEqual:
//	*expr = expr_ref + 3;
//	return c * 3 + '=';
//EndThree:
//	*expr = expr_ref + 2;
//	return c * 3;
//EndTwoEqual:
//	*expr = expr_ref + 2;
//	return c * 2 + '=';
//EndTwo:
//	*expr = expr_ref + 1;
//	return c * 2;
//EndOneEqual:
//	*expr = expr_ref + 1;
//	return c + '=';
//};
//static inline op_t find_next_op_better(const char * *const expr) {
//	uint32_t c;
//	const char* expr_ref = (*expr) - 1;
//repeat_label:
//	++expr_ref;
//	switch (c = *expr_ref) {
//		default:
//			goto repeat_label;
//		case '\0':
//			goto EndZero;
//		case '<': case '>':
//			if (*(expr_ref + 1)) {
//				if (*(expr_ref + 2)) {
//					goto LengthMin3;
//				}
//				goto LengthMin2;
//			}
//			goto EndOne;
//		case '&': case '|': case '!':
//			c += c;
//		case '+': case '-': case '*': case '/': case '%': case '=': case '^': case '?':
//			if (*(expr_ref + 1)) {
//				goto LengthMin2;
//			}
//			goto EndOne;
//		case ':':
//			c += c;
//		case ',': case '~':
//			goto EndOne;
//	}
//	uint32_t temp;
//if (0) {
//LengthMin3:
//	temp = *(uint32_t*)expr_ref;
//	if (temp == ARSA || temp == ALSA) {
//		goto EndThreeEqual;
//	}
//	if (temp == LRSA || temp == LLSA) {
//		goto EndTwoEqual;
//	} else if (temp == ARS || temp == ALS) {
//		goto EndThree;
//	}
//} else {
//LengthMin2:
//	temp = *(uint16_t*)expr_ref;
//}
//	if (temp == LRS || temp == LLS || temp == INC || temp == DEC || temp == EQU || temp == LAND || temp == LOR) {
//		goto EndTwo;
//	}
//	if (temp == ORA || temp == XORA || temp == NEQ || temp == MODA || temp == ANDA || temp == MULA || temp == ADDA || temp == SUBA || temp == DIVA) {
//		goto EndOneEqual;
//	}
//	if (temp == NULC) {
//		c = NullCoalescing;
//	}
//EndOne:
//	*expr = expr_ref;
//EndZero:
//	return c;
//EndThreeEqual:
//	*expr = expr_ref + 3;
//	return c * 3 + '=';
//EndThree:
//	*expr = ++expr_ref + 1;
//	return c * 3;
//EndTwoEqual:
//	*expr = ++expr_ref + 1;
//	return c * 2 + '=';
//EndTwo:
//	*expr = expr_ref + 1;
//	return c * 2;
//EndOneEqual:
//	*expr = expr_ref + 1;
//	return c + '=';
//};
//static inline op_t find_next_op_did_a_dumb(const char * *const expr) {
//	uint32_t c;
//	const char* expr_ref = (*expr) - 1;
//repeat_label:
//	switch (c = *++expr_ref) {
//		default:
//			goto repeat_label;
//		case '\0':
//			break;
//		case ':':
//			*expr = expr_ref;
//			c += c;
//			break;
//		case '&': case '|': case '!':
//			c += c;
//			[[fallthrough]];
//		case '<': case '>':
//		case '+': case '-': case '*': case '/': case '%': case '=': case '^': case '?':
//			uint32_t temp;
//			if (*(expr_ref + 1)) {
//				if ((c == '<' || c == '>') && *(expr_ref + 2)) {
//					temp = *(uint32_t*)expr_ref;
//					if (temp == ARSA || temp == ALSA) {
//						*expr = expr_ref + 3;
//						//*expr = ++expr_ret;
//						//return c * 3 + '=';
//						//expr_ref += 3;
//						c = c * 3 + '=';
//						break;
//					}
//					if (temp == LRSA || temp == LLSA) {
//						*expr = expr_ref + 2;
//						//*expr = expr_ret;
//						//return c * 2 + '=';
//						//expr_ref += 2;
//						c = c * 2 + '=';
//						break;
//					}
//					if (temp == ARS || temp == ALS) {
//						*expr = expr_ref + 2;
//						//*expr = expr_ret;
//						//return c * 3;
//						//expr_ref += 2;
//						c = c * 3;
//						break;
//					}
//					//--expr_ret;
//				} else {
//					temp = *(uint16_t*)expr_ref;
//				}
//				if (temp == LRS || temp == LLS || temp == INC || temp == DEC || temp == EQU || temp == LAND || temp == LOR) {
//					*expr = expr_ref + 1;
//					//*expr = expr_ret;
//					//return c * 2;
//					//expr_ref += 1;
//					c = c * 2;
//					break;
//				}
//				if (temp == ORA || temp == XORA || temp == NEQ || temp == MODA || temp == ANDA || temp == MULA || temp == ADDA || temp == SUBA || temp == DIVA) {
//					*expr = expr_ref + 1;
//					//*expr = expr_ret;
//					//return c + '=';
//					//expr_ref += 1;
//					c = c + '=';
//					break;
//				}
//				if (temp == NULC) {
//					*expr = expr_ref + 1;
//					//*expr = expr_ret;
//					//return NullCoalescing;
//					//expr_ref += 1;
//					c = NullCoalescing;
//					break;
//				}
//			}
//			[[fallthrough]];
//		case ',': case '~':
//			*expr = expr_ref;
//			//return c;
//	}
//	//*expr = expr_ref;
//	return c;
//};
//static inline op_t find_next_op_almost(const char * *const expr) {
//	uint32_t c;
//	const char* expr_ref = (*expr) - 1;
//repeat_label:
//	switch (c = *++expr_ref) {
//		default:
//			goto repeat_label;
//		case '\0':
//			return c;
//		case ':':
//			*expr = expr_ref;
//			return c * 2;
//		case '&': case '|': case '!':
//			c += c;
//			[[fallthrough]];
//		case '<': case '>':
//		case '+': case '-': case '*': case '/': case '%': case '=': case '^': case '?':
//			uint32_t temp;
//			if (*(expr_ref + 1)) {
//				if ((c == '<' || c == '>') && *(expr_ref + 2)) {
//					temp = *(uint32_t*)expr_ref;
//					if (temp == ARSA || temp == ALSA) {
//						*expr = expr_ref + 3;
//						return c * 3 + '=';
//					}
//					if (temp == LRSA || temp == LLSA) {
//						*expr = expr_ref + 2;
//						return c * 2 + '=';
//					}
//					if (temp == ARS || temp == ALS) {
//						*expr = expr_ref + 2;
//						return c * 3;
//					}
//				} else {
//					temp = *(uint16_t*)expr_ref;
//				}
//				if (temp == LRS || temp == LLS || temp == INC || temp == DEC || temp == EQU || temp == LAND || temp == LOR) {
//					*expr = expr_ref + 1;
//					return c * 2;
//				}
//				if (temp == ORA || temp == XORA || temp == NEQ || temp == MODA || temp == ANDA || temp == MULA || temp == ADDA || temp == SUBA || temp == DIVA) {
//					*expr = expr_ref + 1;
//					return c + '=';
//				}
//				if (temp == NULC) {
//					*expr = expr_ref + 1;
//					return NullCoalescing;
//				}
//			}
//			[[fallthrough]];
//		case ',': case '~':
//			*expr = expr_ref;
//			return c;
//	}
//};

static inline const char* find_matching_end(const char* str, char start, char end) {
	if (!str) return NULL;
	size_t depth = 0;
	while (*str) {
		//log_printf("Bracket matching: \"%s\"\n", str);
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

static inline const char* parse_brackets(const char* str, char opening) {
	const char * str_ref = str - 1;
	bool go_to_end = opening == '\0';
	int32_t paren_count = opening == '(';
	int32_t square_count = opening == '[';
	int32_t curly_count = opening == '{';
	char c;
	for (;;) {
		c = *++str_ref;
		if (!c)
			return str_ref;
		paren_count -= c == ')';
		square_count -= c == ']';
		curly_count -= c == '}';
		if (!go_to_end && !paren_count && !square_count && !curly_count)
			return str_ref;
		else if (paren_count < 0 || square_count < 0 || curly_count < 0)
			return str;
		paren_count += c == '(';
		square_count += c == '[';
		curly_count += c == '{';
	}
	/*for (;;) {
		switch (*++str_ref) {
			case '\0':
				return str_ref;
			case '(':
				++paren_count;
				continue;
			case '[':
				++square_count;
				continue;
			case '{':
				++curly_count;
				continue;
			case ')':
				--paren_count;
				break;
			case ']':
				--square_count;
				break;
			case '}':
				--curly_count;
				break;
		}
		if (!paren_count && !square_count && !curly_count) {
			return str_ref;
		}
	}*/
}

static inline bool validate_brackets(const char* str) {
	return parse_brackets(str, '\0') != str;
}

static op_t find_next_op(const char * *const expr) {
	uint32_t c;
	const char* expr_ref = *expr;
	for (;;) {
		switch (c = *expr_ref++) {
			case '\0':
				return c;
			case ':':
				*expr = expr_ref;
				return c * 2;
			case '&': case '|': case '!':
				c += c;
			case '<': case '>':
			case '+': case '-': case '*': case '/': case '%': case '=': case '^': case '?':
				uint32_t temp;
				if (expr_ref[1]) {
					if ((c == '<' || c == '>') && expr_ref[2]) {
						temp = *(uint32_t*)expr_ref;
						if (temp == ARSA || temp == ALSA) {
							*expr = &expr_ref[4];
							return c * 3 + '=';
						}
						if (temp == LRSA || temp == LLSA) {
							*expr = &expr_ref[3];
							return c * 2 + '=';
						}
						if (temp == ARS || temp == ALS) {
							*expr = &expr_ref[3];
							return c * 3;
						}
					} else {
						temp = *(uint16_t*)expr_ref;
					}
					if (temp == LRS || temp == LLS || temp == INC || temp == DEC || temp == EQU || temp == LAND || temp == LOR) {
						*expr = &expr_ref[2];
						return c * 2;
					}
					if (temp == ORA || temp == XORA || temp == NEQ || temp == MODA || temp == ANDA || temp == MULA || temp == ADDA || temp == SUBA || temp == DIVA) {
						*expr = &expr_ref[2];
						return c + '=';
					}
					if (temp == NULC) {
						*expr = &expr_ref[2];
						return NullCoalescing;
					}
				}
			case ',': case '~':
				*expr = &expr_ref[1];
				return c;
		}
	}
}

static op_t __fastcall find_next_op_2(const char * *const expr, const char * *const pre_op_ptr) {
	uint32_t c;
	const char* expr_ref = *expr - 1;
	for (;;) {
		switch (c = *(unsigned char*)++expr_ref) {
			case '\0':
				return c;
			case '(': case '[': /*case '{':*/ {
				const char* end_bracket = parse_brackets(expr_ref, c);
				if (*end_bracket == 'c') {
					return BadBrackets;
				}
				expr_ref = end_bracket + 1;
				continue;
			}
			case ':':
				*pre_op_ptr = expr_ref;
				//*expr = expr_ref + 1;
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
						if (temp == ARSA || temp == ALSA) {
							*pre_op_ptr = expr_ref;
							//*expr = expr_ref + 4;
							*expr = &expr_ref[4];
							return c * 3 + '=';
						}
						if (temp == LRSA || temp == LLSA) {
							*pre_op_ptr = expr_ref;
							//*expr = expr_ref + 3;
							*expr = &expr_ref[3];
							return c * 2 + '=';
						}
						if (temp == ARS || temp == ALS) {
							*pre_op_ptr = expr_ref;
							//*expr = expr_ref + 3;
							*expr = &expr_ref[3];
							return c * 3;
						}
					} else {
						temp = *(uint16_t*)expr_ref;
					}
					if (temp == LRS || temp == LLS || temp == INC || temp == DEC || temp == EQU || temp == LAND || temp == LOR) {
						*pre_op_ptr = expr_ref;
						//*expr = expr_ref + 2;
						*expr = &expr_ref[2];
						return c * 2;
					}
					if (temp == ORA || temp == XORA || temp == NEQ || temp == MODA || temp == ANDA || temp == MULA || temp == ADDA || temp == SUBA || temp == DIVA) {
						*pre_op_ptr = expr_ref;
						//*expr = expr_ref + 2;
						*expr = &expr_ref[2];
						return c + '=';
					}
					if (temp == NULC) {
						*pre_op_ptr = expr_ref;
						//*expr = expr_ref + 2;
						*expr = &expr_ref[2];
						return NullCoalescing;
					}
				}
			case ',': case '~':
				*pre_op_ptr = expr_ref;
				//*expr = expr_ref + 1;
				*expr = &expr_ref[1];
				return c;
		}
	}
}

static inline char OpPrecedence(op_t op) {
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
			return -1;
	}
}

static inline void PrintOp(op_t op) {
	const char * op_text;
	switch (op) {
		case NoOp:
			op_text = "NoOp";
			break;
		case Add:
			op_text = "+";
			break;
		case Subtract:
			op_text = "-";
			break;
		case Multiply:
			op_text = "*";
			break;
		case Divide:
			op_text = "/";
			break;
		case Modulo:
			op_text = "%";
			break;
		case LogicalLeftShift:
			op_text = "<<";
			break;
		case LogicalRightShift:
			op_text = ">>";
			break;
		case ArithmeticLeftShift:
			op_text = "<<<";
			break;
		case ArithmeticRightShift:
			op_text = ">>>";
			break;
		case BitwiseAnd:
			op_text = "&";
			break;
		case BitwiseXor:
			op_text = "^";
			break;
		case BitwiseOr:
			op_text = "|";
			break;
		case BitwiseNot:
			op_text = "~";
			break;
		case LogicalAnd:
			op_text = "&&";
			break;
		case LogicalOr:
			op_text = "||";
			break;
		case LogicalNot:
			op_text = "!";
			break;
		case Less:
			op_text = "<";
			break;
		case LessEqual:
			op_text = "<=";
			break;
		case Greater:
			op_text = ">";
			break;
		case GreaterEqual:
			op_text = ">=";
			break;
		case Equal:
			op_text = "==";
			break;
		case NotEqual:
			op_text = "!=";
			break;
		case Comma:
			op_text = ",";
			break;
		case Increment:
			op_text = "++";
			break;
		case Decrement:
			op_text = "--";
			break;
		case TernaryStart:
			op_text = "?";
			break;
		case TernaryEnd:
			op_text = ":";
			break;
		case NullCoalescing:
			op_text = "?:";
			break;
		case Assign:
			op_text = "=";
			break;
		case AddAssign:
			op_text = "+=";
			break;
		case SubtractAssign:
			op_text = "-=";
			break;
		case MultiplyAssign:
			op_text = "*=";
			break;
		case DivideAssign:
			op_text = "/=";
			break;
		case ModuloAssign:
			op_text = "%=";
			break;
		case LogicalLeftShiftAssign:
			op_text = "<<=";
			break;
		case LogicalRightShiftAssign:
			op_text = ">>=";
			break;
		case ArithmeticLeftShiftAssign:
			op_text = "<<<=";
			break;
		case ArithmeticRightShiftAssign:
			op_text = ">>>=";
			break;
		case AndAssign:
			op_text = "&=";
			break;
		case XorAssign:
			op_text = "^=";
			break;
		case OrAssign:
			op_text = "|=";
			break;
		default:
			op_text = "ERROR";
			break;
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

static inline unsigned char GetOpWidth(op_t op) {
	switch (op) {
		default: case NoOp:
			return 0;
		case Add: case Subtract: case Multiply: case Divide: case Modulo:
		case BitwiseAnd: case BitwiseXor: case BitwiseOr: case BitwiseNot:
		case LogicalNot: case Assign: case Less: case Greater: case Comma:
		case TernaryStart: case TernaryEnd:
			return 1;
		case LogicalLeftShift: case LogicalRightShift: case LogicalAnd:
		case LogicalOr: case Equal: case NotEqual: case LessEqual:
		case GreaterEqual: case NullCoalescing: case AddAssign:
		case SubtractAssign: case MultiplyAssign: case DivideAssign:
		case ModuloAssign: case AndAssign: case OrAssign: case XorAssign:
			return 2;
		case LogicalLeftShiftAssign: case LogicalRightShiftAssign:
		case ArithmeticLeftShift: case ArithmeticRightShift:
			return 3;
		case ArithmeticLeftShiftAssign: case ArithmeticRightShiftAssign:
			return 4;
	}
}

//static inline bool test_for_patch_types(const char* str) {
//
//}
//static inline bool check_for_dang_ternary(const char* expr, char end) {
//	char temp;
//	bool ret;
//	while ((temp = *expr++) != end) {
//		switch (temp) {
//			case '?':
//				return true;
//			case '(':
//				//expr = find_matching_end(expr, )
//				;
//		}
//	}
//	return ret;
//}

size_t eval_expr_new_base(const char **expr_ptr, x86_reg_t *regs, char end, size_t rel_source);

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

static inline size_t CodecaveNotFoundError(const char *const name) {
	log_printf("EXPRESSION ERROR 4: Codecave \"%s\" not found\n", name);
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

//size_t eval_expr_new_impl_(const char **expr_ptr, x86_reg_t *regs, char end, size_t rel_source, bool HasTernary) {
//
//	enum ParsingMode : unsigned char {
//		None,
//		Value,
//		Identifier,
//		ValueOrIdentifier,
//		Operator,
//		Subexpression,
//		Option,
//		Codecave,
//		Address,
//		Dereference,
//		Cast
//	};
//
//	const char *expr = *expr_ptr;
//	size_t value = 0;
//	op_t prev_op = NoOp;
//	op_t next_op = NoOp;
//	ParsingMode mode = Value;
//#define breakpoint_test_var regs
//#define is_breakpoint (breakpoint_test_var)
//#define is_binhack (!breakpoint_test_var)
//#define is_bracket(c) (c == '[')
//
//	/// Parser functions
//		/// ----------------
//	auto consume = [&expr](const stringref_t token) {
//		if (!strnicmp(expr, token.str, token.len)) {
//			expr += token.len;
//			return true;
//		}
//		return false;
//	};
//
//	auto consume_whitespace = [&expr]() {
//		while (expr[0] == ' ' || expr[0] == '\t') {
//			expr++;
//		}
//	};
//
//	auto find_character = [&expr](const char c) {
//		while (*expr != '\0' && *expr != c) {
//			expr++;
//		}
//		return *expr;
//	};
//
//	auto is_reg_name = [regs](const char * *const expr, size_t* value_out) {
//		if (!regs) {
//			return false;
//		}
//		const char *const expr_ref = *expr;
//		if (!expr_ref[0] || !expr_ref[1] || !expr_ref[2]) {
//			return false;
//		}
//		// Since !expr_ref[2] already validated the string has at least
//		// three non-null characters, there must also be at least one
//		// more character because of the null terminator, thus it's fine
//		// to just read a whole 4 bytes at once.
//		uint32_t cmp = *(uint32_t*)expr_ref & 0x00FFFFFF;
//		strlwr((char*)&cmp);
//		switch (cmp) {
//			default:
//				return false;
//			case EAX: *value_out = regs->eax; break;
//			case ECX: *value_out = regs->ecx; break;
//			case EDX: *value_out = regs->edx; break;
//			case EBX: *value_out = regs->ebx; break;
//			case ESP: *value_out = regs->esp; break;
//			case EBP: *value_out = regs->ebp; break;
//			case ESI: *value_out = regs->esi; break;
//			case EDI: *value_out = regs->edi; break;
//		}
//		*expr = &expr_ref[3];
//		return true;
//	};
//
//	auto get_patch_value = [rel_source](const char * *const expr, char opening, size_t* out) {
//		const bool is_relative = opening == '[';
//		//const char ending = opening == '<' ? '>' : ']';
//		const char * expr_ref = *expr;
//		//const char *const end_of_patch_val = find_matching_end(expr_ref, opening, ending);
//		const char *const end_of_patch_val = find_matching_end(expr_ref, opening, is_relative ? ']' : '>');
//		if (!end_of_patch_val) {
//			return false;
//		}
//		const size_t patch_val_length = end_of_patch_val - expr_ref;
//		const char * patch_val_str = end_of_patch_val - patch_val_length;
//		size_t ret;
//		if			(strnicmp(patch_val_str, "option:", 7) == 0) {
//			patch_val_str += 7;
//			ret = 0;
//		} else if	(strnicmp(patch_val_str, "codecave:", 9) == 0) {
//			patch_val_str += 9;
//			ret = 0;
//		} else if	(strnicmp(patch_val_str, "patch:", 6) == 0) {
//			patch_val_str += 6;
//			ret = 0;
//		} else {
//			ret = 0;
//		}
//		*out = ret;
//		*expr = end_of_patch_val;
//		return true;
//	};
//	
//	/// ----------------
//
//	log_printf("Expression: %s\n", expr);
//
//	char current_char;
//	while (current_char = *expr) {
//		while (current_char == ' ' || current_char == '\t') {
//			current_char = *++expr;
//		}
//		size_t cur_value;
//		switch (mode) {
//			case Value: {
//				bool is_negative = current_char == '-';
//				expr = is_negative ? ++expr : expr;
//				switch (*expr) {
//					case '[':
//						if (is_binhack) {
//							if (!get_patch_value(&expr, '[', &cur_value)) {
//								// Bracket error
//								return 0;
//							}
//							break;
//						}
//					case '<':
//						if (!get_patch_value(&expr, '<', &cur_value)) {
//							// Bracket error
//							return 0;
//						}
//						break;
//					case '(':
//						//Recurse
//					case '{':
//						//Recurse
//					default:
//						if (is_breakpoint && is_reg_name(&expr, &cur_value)) {
//							//cur_value is set inside is_reg_name
//						} else {
//							str_address_ret_t addr_ret;
//							cur_value = str_address_value(expr, nullptr, &addr_ret);
//							if (expr == addr_ret.endptr || (addr_ret.error && addr_ret.error != STR_ADDRESS_ERROR_GARBAGE)) {
//								// TODO: Print a message specific to the error code.
//								log_printf("Error while evaluating expression around '%s': unknown character.\n", expr);
//								return 0;
//							}
//							expr = addr_ret.endptr;
//						}
//				}
//				//cur_value = is_negative ? -cur_value : cur_value;
//				mode = Operator;
//				break;
//			}
//			case Operator: {
//				const char * op_ptr = expr;
//				const char * pre_op_ptr;
//				op_t next_op = find_next_op_2(&op_ptr, &pre_op_ptr);
//				if (next_op == BadBrackets) {
//					//Error message
//					return 0;
//				}
//				//size_t current_value_length = pre_op_ptr - expr;
//				expr = op_ptr;
//				if (CompareOpPrecedence(prev_op, next_op) > 0) {
//					//Recurse
//					cur_value = 0;
//				}
//				switch (next_op) {
//					case Increment:
//						IncDecWarning();
//						++value;
//						break;
//					case Decrement:
//						IncDecWarning();
//						--value;
//						break;
//					case LogicalNot:
//						value = !value;
//						break;
//					case BitwiseNot:
//						value = ~value;
//						break;
//					case MultiplyAssign:
//						AssignmentWarning();
//					case Multiply:
//						value *= cur_value;
//						break;
//					case DivideAssign:
//						AssignmentWarning();
//					case Divide:
//						value /= cur_value;
//						break;
//					case ModuloAssign:
//						AssignmentWarning();
//					case Modulo:
//						value %= cur_value;
//						break;
//					case AddAssign:
//						AssignmentWarning();
//					case Add:
//						value += cur_value;
//						break;
//					case SubtractAssign:
//						AssignmentWarning();
//					case Subtract:
//						value -= cur_value;
//						break;
//					case LogicalLeftShiftAssign:
//					case ArithmeticLeftShiftAssign:
//						AssignmentWarning();
//					case LogicalLeftShift:
//					case ArithmeticLeftShift:
//						value <<= cur_value;
//						break;
//					case LogicalRightShiftAssign:
//						AssignmentWarning();
//					case LogicalRightShift:
//						value >>= cur_value;
//						break;
//					case ArithmeticRightShiftAssign:
//						AssignmentWarning();
//					case ArithmeticRightShift:
//						value = (uint32_t)((int32_t)value >> cur_value);
//						break;
//					case Less:
//						value = value < cur_value;
//						break;
//					case LessEqual:
//						value = value <= cur_value;
//						break;
//					case Greater:
//						value = value > cur_value;
//						break;
//					case GreaterEqual:
//						value = value >= cur_value;
//						break;
//					case Equal:
//						value = value == cur_value;
//						break;
//					case NotEqual:
//						value = value != cur_value;
//						break;
//					case BitwiseAnd:
//						value &= cur_value;
//						break;
//					case BitwiseXor:
//						value ^= cur_value;
//						break;
//					case BitwiseOr:
//						value |= cur_value;
//						break;
//					case LogicalAnd:
//						value = value && cur_value;
//						break;
//					case LogicalOr:
//						value = value || cur_value;
//						break;
//					case TernaryStart:
//						TernaryWarning();
//						break;
//					case NullCoalescing:
//						TernaryWarning();
//						break;
//					case Assign:
//						AssignmentWarning();
//						value = cur_value;
//						break;
//					case Comma:
//						//why tho
//						break;
//				}
//			}
//		}
//
//		++expr;
//	}
//
//	//op_t op = find_next_op(&op_ptr);
//	//op_ptr += GetOpWidth(op);
//	//const char * next_op_ptr = op_ptr;
//	//op_t next_op = find_next_op(&next_op_ptr);
//	//next_op_ptr += GetOpWidth(next_op);
//	//
//	//size_t cur_value;
//	//
//	//auto ptr_size = sizeof(size_t);
//	//if (consume("byte ptr")) {
//	//	ptr_size = 1;
//	//} else if (consume("word ptr")) {
//	//	ptr_size = 2;
//	//} else if (consume("dword ptr")) {
//	//	ptr_size = 4;
//	//}
//	//consume_whitespace();
//	//
//	//
//	//const char* func_type = NULL;
//	//char func_end;
//	//switch (*expr) {
//	//	case '(':
//	//		expr++;
//	//		mode = Subexpression;
//	//		break;
//	//	case '[':
//	//		if (is_breakpoint) {
//	//			expr++;
//	//			mode = Dereference;
//	//			break;
//	//		}
//	//	case '<':
//	//		func_end = *expr;
//	//		if (consume("option:")) {
//	//			expr++;
//	//			mode = Option;
//	//			func_type = "option";
//	//			break;
//	//		} else if (consume("codecave:")) {
//	//			expr++;
//	//			mode = Codecave;
//	//			func_type = "codecave";
//	//			break;
//	//		} else if (consume("0x") || consume("Rx") || consume("address:")) {
//	//			expr++;
//	//			mode = Address;
//	//			func_type = "address";
//	//			break;
//	//		}
//	//	case '>':
//	//	case '+': case '-': case '*': case '/': case '%':
//	//	case '|': case '&': case '^':
//	//		expr++;
//	//		if (*expr == '=')
//	//	case '!': case '~':
//	//	case '=':
//	//	default:
//	//		mode = Value;
//	//		break;
//	//}
//	//switch (mode) {
//	//	case Subexpression:
//	//	{
//	//		const char* end_match = find_matching_end(expr, '(', ')');
//	//		ptrdiff_t matched_length = (end_match - 1) - (expr + 1);
//	//		cur_value = eval_expr_new_base(&expr, regs, matched_length, rel_source);
//	//		checked_length += matched_length;
//	//		break;
//	//	}
//	//	case Dereference:
//	//	{
//	//		const char* end_match = find_matching_end(expr, '[', ']');
//	//		ptrdiff_t matched_length = (end_match - 1) - (expr + 1);
//	//		cur_value = eval_expr_new_base(&expr, regs, matched_length, rel_source);
//	//		checked_length += matched_length;
//	//		if (cur_value) {
//	//			//cur_value = dereference(cur_value, ptr_size);
//	//		}
//	//		break;
//	//	}
//	//	case Option: case Codecave: case Address:
//	//	{
//	//		const char *const fs = expr;
//	//		if (!find_character(func_end)) {
//	//			log_printf("ERROR: %s not terminated, missing '>'!\n", func_type);
//	//			return 0;
//	//		}
//	//		size_t func_name_len = expr - fs;
//	//		VLA(char, func_name, func_name_len);
//	//		strncpy(func_name, fs, func_name_len);
//	//		func_name[func_name_len] = '\0';
//	//		union {
//	//			size_t address;
//	//			patch_opt_val_t* option;
//	//		} func_val;
//	//		switch (mode) {
//	//			case Option:
//	//				func_val.option = patch_opt_get(func_name);
//	//				break;
//	//			case Codecave:
//	//				func_val.address = (size_t)func_get(func_name);
//	//				break;
//	//			case Address:
//	//				str_address_ret_t address_error;
//	//				size_t address = str_address_value(func_name, NULL, &address_error);
//	//				if (address && address_error.error == STR_ADDRESS_ERROR_NONE) {
//	//					func_val.address = address;
//	//				} else {
//	//					func_val.address = 0;
//	//				}
//	//				break;
//	//		}
//	//		if (!func_val.address) {
//	//			log_printf("ERROR: %s %s not found\n", func_type, func_name);
//	//		} else if (mode != Option) {
//	//			char* offset_string = strchr(func_name, '+');
//	//			if (offset_string && strlen(offset_string) > 1) {
//	//				*offset_string = '\0';
//	//				offset_string++;
//	//				func_val.address += eval_expr_new_base((const char**)&offset_string, regs, '\0', NULL);
//	//			}
//	//		}
//	//		VLA_FREE(func_name);
//	//		if (mode != Option) {
//	//			cur_value = func_val.address;
//	//		} else {
//	//			switch (func_val.option->t) {
//	//				case PATCH_OPT_VAL_BYTE: cur_value = func_val.option->val.byte; break;
//	//				case PATCH_OPT_VAL_WORD: cur_value = func_val.option->val.word; break;
//	//				case PATCH_OPT_VAL_DWORD: cur_value = func_val.option->val.dword; break;
//	//				case PATCH_OPT_VAL_FLOAT: cur_value = (size_t)func_val.option->val.f; break;
//	//				case PATCH_OPT_VAL_DOUBLE: cur_value = (size_t)func_val.option->val.d; break;
//	//			}
//	//		}
//	//		break;
//	//	}
//	//	case Value:
//	//	{
//	//		
//	//	}
//	//}
//	//
//	//
//	//
//	//
//	//	if (*expr == '(') {
//	//		expr++;
//	//
//	//	} else if (*expr == '<' && consume("<option:")) {
//	//		const char *const fs = expr;
//	//		if (!find_character('>')) {
//	//			log_printf("ERROR: option name not terminated!\n");
//	//			return 0;
//	//		}
//	//		size_t opt_name_len = expr - fs;
//	//		VLA(char, opt_name, opt_name_len);
//	//		strncpy(opt_name, fs, opt_name_len);
//	//		opt_name[opt_name_len] = '\0';
//	//		patch_opt_val_t* option = patch_opt_get(opt_name);
//	//		if (!option) {
//	//			log_printf("ERROR: option %s not found\n", opt_name);
//	//			return 0;
//	//		}
//	//		VLA_FREE(opt_name);
//	//		switch (option->t) {
//	//			case PATCH_OPT_VAL_BYTE: cur_value = option->val.byte; break;
//	//			case PATCH_OPT_VAL_WORD: cur_value = option->val.word; break;
//	//			case PATCH_OPT_VAL_DWORD: cur_value = option->val.dword; break;
//	//			case PATCH_OPT_VAL_FLOAT: cur_value = (size_t)option->val.f; break;
//	//			case PATCH_OPT_VAL_DOUBLE: cur_value = (size_t)option->val.d; break;
//	//		}
//	//		++expr;
//	//	} else if (*expr == '<' || *expr == '[') {
//	//		bool func_rel = *expr == '[';
//	//		const char *const fs = expr;
//	//		while (expr && *expr != '>') {
//	//			++expr;
//	//		}
//	//		if (!expr) {
//	//			log_printf("ERROR: option name not terminated!\n");
//	//			return 0;
//	//		}
//	//		size_t opt_name_len = expr - fs;
//	//		VLA(char, opt_name, opt_name_len);
//	//		strncpy(opt_name, fs, opt_name_len);
//	//		opt_name[opt_name_len] = '\0';
//	//		patch_opt_val_t* option = patch_opt_get(opt_name);
//	//		if (!option) {
//	//			log_printf("ERROR: option %s not found\n", opt_name);
//	//			return 0;
//	//		}
//	//		VLA_FREE(opt_name);
//	//		++expr;
//	//	} else if (*expr == '[') {
//	//
//	//	} else if (regs && (cur_value = (size_t)reg(regs, expr, &expr)) != 0) {
//	//		//cur_value = dereference(cur_value, ptr_size);
//	//	} else {
//	//		str_address_ret_t addr_ret;
//	//		cur_value = str_address_value(expr, nullptr, &addr_ret);
//	//		if (expr == addr_ret.endptr || (addr_ret.error && addr_ret.error != STR_ADDRESS_ERROR_GARBAGE)) {
//	//			// TODO: Print a message specific to the error code.
//	//			log_printf("Error while evaluating expression around '%s': unknown character.\n", expr);
//	//			return 0;
//	//		}
//	//		expr = addr_ret.endptr;
//	//	}
//	//
//	//if (0) {
//	//	
//	//	//Option value is read further down
//	//} else {
//	//	consume_whitespace();
//	//	if (consume("==")) {
//	//		op = EQ;
//	//		continue;
//	//	} else if (consume("!=")) {
//	//		op = NE;
//	//		continue;
//	//	} else if (consume("<")) {
//	//		if (consume("=")) {
//	//			op = LE;
//	//		} else if (consume("<")) {
//	//			op = LS;
//	//		} else if (consume("option:")) {
//	//			fs = expr;
//	//		} else {
//	//			op = '<';
//	//		}
//	//		continue;
//	//	} else if (consume(">=")) {
//	//		op = GE;
//	//		continue;
//	//	} else if (consume(">>")) {
//	//		op = RS;
//	//		continue;
//	//	} else if (consume("||")) {
//	//		op = LO;
//	//		continue;
//	//	} else if (consume("&&")) {
//	//		op = LA;
//	//		continue;
//	//	} else if (consume("?")) {
//	//		if (consume("?") || consume(":")) {
//	//			op = NC;
//	//		} else {
//	//			op = TC;
//	//		}
//	//		continue;
//	//	} else if (strchr("+-*/%>|&^~!", *expr)) {
//	//		op = *expr;
//	//		expr++;
//	//		continue;
//	//	}
//	//}
//	//
//	//	switch (op) {
//	//		case Add:
//	//			value += cur_value;
//	//			break;
//	//		case Subtract:
//	//			value -= cur_value;
//	//			break;
//	//		case Multiply:
//	//			value *= cur_value;
//	//			break;
//	//		case Divide:
//	//			value /= cur_value;
//	//			break;
//	//		case Modulo:
//	//			value %= cur_value;
//	//			break;
//	//		case Less:
//	//			value = value < cur_value;
//	//			break;
//	//		case Greater:
//	//			value = value > cur_value;
//	//			break;
//	//		case BitwiseOr:
//	//			value |= cur_value;
//	//			break;
//	//		case BitwiseAnd:
//	//			value &= cur_value;
//	//			break;
//	//		case BitwiseXor:
//	//			value ^= cur_value;
//	//			break;
//	//		case BitwiseNot:
//	//			value = ~cur_value;
//	//			break;
//	//		case LogicalNot:
//	//			value = !cur_value;
//	//			break;
//	//		case Equal:
//	//			value = value == cur_value;
//	//			break;
//	//		case NotEqual:
//	//			value = value != cur_value;
//	//			break;
//	//		case LessEqual:
//	//			value = value <= cur_value;
//	//			break;
//	//		case GreaterEqual:
//	//			value = value >= cur_value;
//	//			break;
//	//		case LogicalLeftShift:
//	//			value <<= cur_value;
//	//			break;
//	//		case LogicalRightShift:
//	//			value >>= cur_value;
//	//			break;
//	//		case LogicalOr:
//	//			value = value || cur_value;
//	//			break;
//	//		case LogicalAnd:
//	//			value = value && cur_value;
//	//			break;
//	//			/*case TC:
//	//				if (value) {
//	//
//	//				}
//	//				break;
//	//			case NC:
//	//				value = value ? value : cur_value;
//	//				break;*/
//	//	}
//	//}
//	//
//	//if (end == ']' && *expr != end) {
//	//	log_printf("Error while evaluating expression around '%s': '[' without matching ']'.\n", expr);
//	//	return 0;
//	//}
//	//
//	//expr++;
//	//*expr_ptr = expr;
//	//return value;
//}

//size_t eval_expr_new_impl__(const char **expr_ptr, x86_reg_t *regs, char end, size_t rel_source) {
//
//	enum ParsingMode : unsigned char {
//		None,
//		Value,
//		Identifier,
//		ValueOrIdentifier,
//		Operator,
//		Subexpression,
//		Option,
//		Codecave,
//		Address,
//		Dereference,
//		Cast
//	};
//
//	enum CastType : unsigned char {
//		i8,
//		i16,
//		i32,
//		u8,
//		u16,
//		u32,
//		b,
//		f32
//	};
//
//	const char *expr = *expr_ptr;
//
//	log_printf("START SUBEXPRESSION: \"%s\"\n", expr);
//
//	size_t value = 0;
//	op_t prev_op = NoOp;
//	op_t next_op = NoOp;
//	ParsingMode mode = Value;
//#define breakpoint_test_var regs
//#define is_breakpoint (breakpoint_test_var)
//#define is_binhack (!breakpoint_test_var)
//#define is_bracket(c) (c == '[')
//
//	/// Parser functions
//		/// ----------------
//	auto consume = [&expr](const stringref_t token) {
//		if (!strnicmp(expr, token.str, token.len)) {
//			expr += token.len;
//			return true;
//		}
//		return false;
//	};
//
//	/*auto consume_whitespace = [&expr]() {
//		while (expr[0] == ' ' || expr[0] == '\t') {
//			expr++;
//		}
//	};*/
//
//#define consume_whitespace() for (; current_char == ' ' || current_char == '\t'; current_char = *++expr)
//
//	auto find_character = [&expr](const char c) {
//		while (*expr != '\0' && *expr != c) {
//			expr++;
//		}
//		return *expr;
//	};
//
//	auto is_reg_name = [regs](const char * *const expr, size_t* value_out) {
//		if (!regs) {
//			return false;
//		}
//		const char *const expr_ref = *expr;
//		if (!expr_ref[0] || !expr_ref[1] || !expr_ref[2]) {
//			return false;
//		}
//		// Since !expr_ref[2] already validated the string has at least
//		// three non-null characters, there must also be at least one
//		// more character because of the null terminator, thus it's fine
//		// to just read a whole 4 bytes at once.
//		uint32_t cmp = *(uint32_t*)expr_ref & 0x00FFFFFF;
//		strlwr((char*)&cmp);
//		switch (cmp) {
//			default:
//				return false;
//			case EAX: *value_out = regs->eax; break;
//			case ECX: *value_out = regs->ecx; break;
//			case EDX: *value_out = regs->edx; break;
//			case EBX: *value_out = regs->ebx; break;
//			case ESP: *value_out = regs->esp; break;
//			case EBP: *value_out = regs->ebp; break;
//			case ESI: *value_out = regs->esi; break;
//			case EDI: *value_out = regs->edi; break;
//		}
//		*expr = &expr_ref[3];
//		log_printf("Found register \"%s\"\n", (char*)&cmp);
//		return true;
//	};
//
//	auto get_patch_value = [rel_source, regs](const char * *const expr, char opening, size_t* out) {
//		bool is_relative = opening == '[';
//		//const char ending = opening == '<' ? '>' : ']';
//		const char * expr_ref = *expr;
//		//const char *const end_of_patch_val = find_matching_end(expr_ref, opening, ending);
//		//log_printf("Bracket match start: \"%s\"\n", expr_ref);
//		const char *const end_of_patch_val = find_matching_end(expr_ref, opening, is_relative ? ']' : '>');
//		//log_printf("Bracket match end: \"%s\"\n", end_of_patch_val);
//		if (!end_of_patch_val) {
//			return false;
//		}
//		++expr_ref;
//		size_t patch_val_length = end_of_patch_val - expr_ref;
//		const char * patch_val_str = end_of_patch_val - patch_val_length;
//		size_t ret = 0;
//		if (strnicmp(patch_val_str, "option:", 7) == 0) {
//			patch_val_str += 7;
//			patch_val_length -= 7;
//			char *const name_buffer = strndup(patch_val_str, patch_val_length);
//			log_printf("Found option %s\n", name_buffer);
//			patch_opt_val_t *const option = patch_opt_get(name_buffer);
//			if (!option) {
//				*out = OptionNotFoundError(name_buffer);
//				free(name_buffer);
//				return false;
//			}
//			free(name_buffer);
//			memcpy(&ret, option->val.byte_array, option->size);
//			is_relative = false;
//		} else if (strnicmp(patch_val_str, "codecave:", 9) == 0) {
//			patch_val_str += 9;
//			patch_val_length -= 9;
//			char *const name_buffer = strndup(patch_val_str, patch_val_length);
//			log_printf("Found codecave %*c\n", name_buffer);
//			ret = (size_t)func_get(name_buffer);
//			if (!ret) {
//				*out = CodecaveNotFoundError(name_buffer);
//				free(name_buffer);
//				return false;
//			}
//			free(name_buffer);
//		} else if (strnicmp(patch_val_str, "patch:", 6) == 0) {
//			patch_val_str += 6;
//			patch_val_length -= 6;
//			log_printf("Found patch %*c\n", patch_val_length, patch_val_str);
//			ret = 0;
//			is_relative = false;
//		} else {
//			ret = eval_expr_new_impl(&patch_val_str, regs, is_relative ? ']' : '>', rel_source);
//		}
//		if (is_relative) {
//			ret -= rel_source + sizeof(void*);
//		}
//		/*} else {
//			log_printf("Found invalid %*c\n", patch_val_length, patch_val_str);
//			ret = 0;
//		}*/
//		*out = ret;
//		*expr = end_of_patch_val;
//		return true;
//	};
//
//	/// ----------------
//	size_t cur_value;
//	//char current_char;
//	//--expr;
//	//while ((current_char = *++expr) != end) {
//	for (char current_char = *expr;
//		 current_char != end;
//		 current_char = *++expr) {
//		//
//		consume_whitespace();
//		//
//		log_printf("Remaining expression: \"%s\"\n", expr);
//		//
//		switch (mode) {
//			case Value: {
//				bool is_negative = current_char == '-';
//				expr += is_negative;
//				current_char = *expr;
//				unsigned char ptr_size = sizeof(int32_t);
//				CastType cast_type = u32;
//				switch (current_char) {
//					case 'b': case 'B':
//						if (consume("byte ptr")) {
//							ptr_size = 1;
//							consume_whitespace();
//						}
//						break;
//					case 'w': case 'W':
//						if (consume("word ptr")) {
//							ptr_size = 2;
//							consume_whitespace();
//						}
//						break;
//					case 'd': case 'D':
//						if (consume("dword ptr")) {
//							ptr_size = 4;
//							consume_whitespace();
//						}
//						break;
//					case 'f': case 'F':
//						if (consume("float ptr")) {
//							ptr_size = 4;
//							consume_whitespace();
//						}
//						break;
//				}
//				switch (current_char) {
//					case '(':
//						++expr;
//						if (consume("i8")) {
//							cast_type = i8;
//						} else if (consume("u8")) {
//							cast_type = u8;
//						} else if (consume("i16")) {
//							cast_type = i16;
//						} else if (consume("u16")) {
//							cast_type = u16;
//						} else if (consume("i32")) {
//							cast_type = i32;
//						} else if (consume("u32")) {
//							cast_type = u32;
//						} else if (consume("bool")) {
//							cast_type = b;
//						} else if (consume("f32")) {
//							cast_type = f32;
//						} else {
//							cur_value = eval_expr_new_impl(&expr, regs, ')', rel_source);
//						}
//						
//						break;
//					case '[':
//						if (is_breakpoint) {
//							++expr;
//							cur_value = eval_expr_new_impl(&expr, regs, ']', rel_source);
//							memcpy(&cur_value, (void*)cur_value, ptr_size);
//							break;
//						}
//					case '<':
//						if (!get_patch_value(&expr, current_char, &cur_value)) {
//							return ValueBracketError();
//						}
//						log_printf("Parsed patch value is %X / %d / %u\n", cur_value, cur_value, cur_value);
//						break;
//					case '{':
//					default:
//						if (is_breakpoint && is_reg_name(&expr, &cur_value)) {
//							//cur_value is set inside is_reg_name
//							//log_printf("Register value was %X\n", cur_value);
//						} else {
//							str_address_ret_t addr_ret;
//							cur_value = str_address_value(expr, nullptr, &addr_ret);
//							if (expr == addr_ret.endptr || (addr_ret.error && addr_ret.error != STR_ADDRESS_ERROR_GARBAGE)) {
//								return BadCharacterError();
//							}
//							log_printf("Raw value was %X / %d / %u\n", cur_value, cur_value, cur_value);
//							expr = addr_ret.endptr - 1;
//							log_printf("Remaining after raw value: \"%s\"\n", addr_ret.endptr);
//							//log_printf("Constant value was %X\n", cur_value);
//						}
//				}
//				cur_value = is_negative ? cur_value * -1: cur_value;
//				log_printf("Value was %X / %d / %u\n", cur_value, cur_value, cur_value);
//				value = ApplyOperator(prev_op, value, cur_value);
//				log_printf("Result of operator was %X / %d / %u\n", value, value, value);
//				mode = Operator;
//				break;
//			}
//			case Operator: {
//				const char * op_ptr = expr;
//				const char * pre_op_ptr;
//				next_op = find_next_op_2(&op_ptr, &pre_op_ptr);
//				if (next_op == BadBrackets) {
//					return ValueBracketError();
//				}
//				//size_t current_value_length = pre_op_ptr - expr;
//				//size_t current_op_length = op_ptr - pre_op_ptr;
//				//log_printf("Found operator: \"%*c\"\n", current_op_length, pre_op_ptr);
//				PrintOp(next_op);
//				expr = op_ptr;
//				if (CompareOpPrecedence(prev_op, next_op) > 0) {
//					cur_value = eval_expr_new_impl(&expr, regs, end, rel_source);
//				}
//				
//				
//				prev_op = next_op;
//				mode = Value;
//				break;
//			}
//		}
//	}
//	/*if (prev_op == NoOp && next_op == NoOp) {
//		value = cur_value;
//	}*/
//	log_printf("END SUBEXPRESSION\n");
//	log_printf("Subexpression value was %X / %d / %u\n", value, value, value);
//	*expr_ptr = expr;
//	return value;
//}

static inline bool consume(const char * *const str, const stringref_t token) {
	if (!!strnicmp(*str, token.str, token.len)) return false;
	*str += token.len;
	return true;
}

size_t eval_expr_new_impl(const char **expr_ptr, x86_reg_t *regs, char end, size_t rel_source) {

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
			case EAX: *value_out = regs->eax; break;
			case ECX: *value_out = regs->ecx; break;
			case EDX: *value_out = regs->edx; break;
			case EBX: *value_out = regs->ebx; break;
			case ESP: *value_out = regs->esp; break;
			case EBP: *value_out = regs->ebp; break;
			case ESI: *value_out = regs->esi; break;
			case EDI: *value_out = regs->edi; break;
		}
		*expr = &expr_ref[3];
		log_printf("Found register \"%s\"\n", (char*)&cmp);
		return true;
	};

	auto get_patch_value = [rel_source, regs](const char * *const expr, char opening, size_t* out) {
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
		if (consume(&expr_ref, "codecave:")) {
			name_buffer = strndup(expr_ref, PtrDiffStrlen(patch_val_end, expr_ref));
			//log_printf("Found codecave %s\n", name_buffer);
			ret = func_get(name_buffer);
			if (!ret) {
				ret = CodecaveNotFoundError(name_buffer);
				success_state = false;
			}
		} else {
			is_relative = false;
			if (consume(&expr_ref, "option:")) {
				name_buffer = strndup(expr_ref, PtrDiffStrlen(patch_val_end, expr_ref));
				//log_printf("Found option %s\n", name_buffer);
				patch_opt_val_t *const option = patch_opt_get(name_buffer);
				if (option) {
					memcpy(&ret, option->val.byte_array, option->size);
				} else {
					ret = OptionNotFoundError(name_buffer);
					success_state = false;
				}
			} else if (consume(&expr_ref, "patch:")) {
				name_buffer = strndup(expr_ref, PtrDiffStrlen(patch_val_end, expr_ref));
				//log_printf("Found patch %s\n", name_buffer);
				ret = 0;
			} else {
				ret = eval_expr_new_impl(&expr_ref, regs, is_relative ? ']' : '>', rel_source);
			}
		}
		free(name_buffer);
		if (is_relative) {
			ret -= rel_source + sizeof(void*);
		}
		*out = ret;
		*expr = patch_val_end;
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
			if (cast == I32P || cast == i32P) { expr += 3; return i32_cast; }
			if (cast == U32P || cast == u32P) { expr += 3; return u32_cast; }
			if (cast == F32P || cast == f32P) { expr += 3; return f32_cast; }
			if (cast == I16P || cast == i16P) { expr += 3; return i16_cast; }
			if (cast == U16P || cast == u16P) { expr += 3; return u16_cast; }
			cast &= 0x00FFFFFF;
			if (cast == I8P || cast == i8P) { expr += 2; return i8_cast; }
			if (cast == U8P || cast == u8P) { expr += 2; return u8_cast; }
		}
		return invalid_cast;
	};

	/// ----------------
	size_t cur_value;
	for (current_char = *expr; current_char != end; current_char = *++expr) {
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
							return ValueBracketError();
						}
						log_printf("Parsed patch value is %X / %d / %u\n", cur_value, cur_value, cur_value);
						break;
					case '{':
					default:
						if (is_breakpoint && is_reg_name(&expr, &cur_value)) {
							//cur_value is set inside is_reg_name
							//log_printf("Register value was %X\n", cur_value);
						} else {
							str_address_ret_t addr_ret;
							cur_value = str_address_value(expr, nullptr, &addr_ret);
							if (expr == addr_ret.endptr || (addr_ret.error && addr_ret.error != STR_ADDRESS_ERROR_GARBAGE)) {
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
				next_op = find_next_op_2(&op_ptr, &pre_op_ptr);
				if (next_op == BadBrackets) {
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

size_t eval_expr_new_base(const char **expr_ptr, x86_reg_t *regs, char end, size_t rel_source) {
	return eval_expr_new_impl(expr_ptr, regs, end, rel_source/*, check_for_dang_ternary(*expr_ptr, end)*/);
}

size_t eval_expr_new(const char **expr_ptr, x86_reg_t *regs, char end, size_t rel_source) {
	log_printf("START EXPRESSION\n");
	const size_t ret = eval_expr_new_impl(expr_ptr, regs, '\0', rel_source);
	log_printf("END EXPRESSION\nFinal result was: %X / %d / %u\n", ret, ret, ret);
	return ret;
	//if (validate_brackets(*expr_ptr)) {
	//	return eval_expr_new_base(expr_ptr, regs, end, rel_source);
	//} else {
	//	//Bracket error
	//	return 0;
	//}
}

#undef consume_whitespace

size_t eval_expr(const char **expr_ptr, x86_reg_t *regs, char end)
{
	const char *expr = *expr_ptr;
	const char *fs = NULL;
	patch_opt_val_t *option = NULL;
	size_t value = 0;
	unsigned char op = '+';

	enum twochar_op_t : unsigned char {
		EQ = 0x80,
		NE,
		LE,
		GE
	};

	/// Parser functions
	/// ----------------
	auto consume = [&expr] (const stringref_t token) {
		if(!strnicmp(expr, token.str, token.len)) {
			expr += token.len;
			return true;
		}
		return false;
	};

	auto consume_whitespace = [&expr] () {
		while(expr[0] == ' ' || expr[0] == '\t') {
			expr++;
		}
	};

	auto dereference = [] (size_t addr, size_t len) {
		size_t ret = 0;
		memcpy(&ret, reinterpret_cast<void *>(addr), len);
		return ret;
	};
	/// ----------------

	while (*expr && *expr != end) {
		if (fs) {
			if (*expr != '>') {
				++expr;
				continue;
			}
			size_t opt_name_len = expr - fs;
			VLA(char, opt_name, opt_name_len);
			strncpy(opt_name, fs, opt_name_len);
			opt_name[opt_name_len] = 0;
			option = patch_opt_get(opt_name);
			if (!option) {
				log_printf("ERROR: option %s not found\n", opt_name);
			}
			VLA_FREE(opt_name);
			fs = NULL;
		} else {
			consume_whitespace();
			if (consume("==")) {
				op = EQ;
				continue;
			} else if (consume("!=")) {
				op = NE;
				continue;
			} else if (consume("<")) {
				if (consume("=")) {
					op = LE;
				} else if (consume("option:")) {
					fs = expr;
				} else {
					op = '<';
				}
				continue;
			} else if (consume(">=")) {
				op = GE;
				continue;
			} else if (strchr("+-*/%>", *expr)) {
				op = *expr;
				expr++;
				continue;
			}
		}

		auto ptr_size = sizeof(size_t);
		if(consume("byte ptr")) {
			ptr_size = 1;
		} else if(consume("word ptr")) {
			ptr_size = 2;
		} else if(consume("dword ptr")) {
			ptr_size = 4;
		}
		consume_whitespace();

		size_t cur_value;
		if (*expr == '(') {
			expr++;
			cur_value = eval_expr(&expr, regs, ')');
		}
		else if (*expr == '[') {
			expr++;
			cur_value = eval_expr(&expr, regs, ']');
			if (cur_value) {
				cur_value = dereference(cur_value, ptr_size);
			}
		}
		else if ((cur_value = (size_t)reg(regs, expr, &expr)) != 0) {
			cur_value = dereference(cur_value, ptr_size);
		}
		else if (option) {
			switch (option->t) {
				case PATCH_OPT_VAL_BYTE: cur_value = option->val.byte; break;
				case PATCH_OPT_VAL_WORD: cur_value = option->val.word; break;
				case PATCH_OPT_VAL_DWORD: cur_value = option->val.dword; break;
				case PATCH_OPT_VAL_FLOAT: cur_value = (size_t)option->val.f; break;
				case PATCH_OPT_VAL_DOUBLE: cur_value = (size_t)option->val.d; break;
			}
			option = NULL;
			++expr;
		}
		else {
			str_address_ret_t addr_ret;
			cur_value = str_address_value(expr, nullptr, &addr_ret);
			if (expr == addr_ret.endptr || (addr_ret.error && addr_ret.error != STR_ADDRESS_ERROR_GARBAGE)) {
				// TODO: Print a message specific to the error code.
				log_printf("Error while evaluating expression around '%s': unknown character.\n", expr);
				return 0;
			}
			expr = addr_ret.endptr;
		}

		switch (op) {
		case '+':
			value += cur_value;
			break;
		case '-':
			value -= cur_value;
			break;
		case '*':
			value *= cur_value;
			break;
		case '/':
			value /= cur_value;
			break;
		case '%':
			value %= cur_value;
			break;
		case '<':
			value = value < cur_value;
			break;
		case '>':
			value = value > cur_value;
			break;
		case EQ:
			value = value == cur_value;
			break;
		case NE:
			value = value != cur_value;
			break;
		case LE:
			value = value <= cur_value;
			break;
		case GE:
			value = value >= cur_value;
			break;
		}
	}

	if (end == ']' && *expr != end) {
		log_printf("Error while evaluating expression around '%s': '[' without matching ']'.\n", expr);
		return 0;
	}

	expr++;
	*expr_ptr = expr;
	return value;
}
