/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Binary hack handling.
  */

#include "thcrap.h"
#include <math.h>
#include <locale.h>

/*
 * Grumble, grumble, C is garbage and will only do string→float conversion
 * using the decimal separator from the current locale, and OF COURSE it never
 * occured to anyone, not even Microsoft, to provide a neutral strtod() that
 * always looks for a decimal point, and so we have to dynamically allocate
 * (and free) The Neutral Locale instead. C is garbage.
 */
struct LocaleHolder_t {
	_locale_t locale = nullptr;

	LocaleHolder_t(int category, const char* locale_str) {
		locale = _create_locale(category, locale_str);
	}

	~LocaleHolder_t() {
		if (locale) {
			_free_locale(locale);
			locale = nullptr;
		}
	}
};

static LocaleHolder_t lc_neutral(LC_NUMERIC, "C");

void hackpoints_error_function_not_found(const char *func_name)
{
	if (runconfig_msgbox_invalid_func()) {
		if (log_mboxf("Hackpoint error", MB_OKCANCEL | MB_ICONERROR, "ERROR: function '%s' not found! ", func_name) == IDCANCEL) {
			thcrap_ExitProcess(0);
		}
	} else {
		log_printf("ERROR: function '%s' not found! "
#ifdef _DEBUG
			"(implementation not exported or still missing?)"
#else
			"(outdated or corrupt %s installation, maybe?)"
#endif
			, func_name, PROJECT_NAME_SHORT
		);
	}
}

TH_CALLER_DELETEA hackpoint_addr_t* hackpoint_addrs_from_json(json_t* addr_array)
{

	if (!addr_array) {
		return NULL;
	}

	size_t addr_count = 0;

	json_t *it;
	json_flex_array_foreach_scoped(size_t, i, addr_array, it) {
		if (json_is_integer(it) || json_is_string(it)) {
			++addr_count;
		}
	}
	if (!addr_count) {
		return NULL;
	}

	hackpoint_addr_t* ret = new hackpoint_addr_t[addr_count + 1];
	ret[addr_count].str = NULL;
	ret[addr_count].raw = 0;
	ret[addr_count].type = END_ADDR;
	ret[addr_count].binhack_source = NULL;

	addr_count = 0;

	json_flex_array_foreach_scoped(size_t, i, addr_array, it) {
		if (json_is_string(it)) {
			ret[addr_count].str = json_string_copy(it);
			ret[addr_count].raw = 0;
			ret[addr_count].type = STR_ADDR;
			ret[addr_count].binhack_source = NULL;
			++addr_count;
		}
		else if (json_is_integer(it)) {
			ret[addr_count].str = NULL;
			ret[addr_count].raw = (uintptr_t)json_integer_value(it);
			ret[addr_count].type = RAW_ADDR;
			ret[addr_count].binhack_source = NULL;
			++addr_count;
		}
	}

	return ret;
}

inline bool eval_hackpoint_addr(hackpoint_addr_t* hackpoint_addr, uintptr_t* out, HMODULE hMod)
{
	if (hackpoint_addr) {
		uintptr_t addr = 0;
		switch (int8_t addr_type = hackpoint_addr->type) {
			case STR_ADDR:
				eval_expr(hackpoint_addr->str, '\0', &addr, NULL, (uintptr_t)hMod);
				hackpoint_addr->raw = addr;
				[[fallthrough]];
			case RAW_ADDR:
				addr = hackpoint_addr->raw;
				if (!addr) {
					hackpoint_addr->type = INVALID_ADDR;
				}
				[[fallthrough]];
			case INVALID_ADDR:
				*out = addr;
				return true;
			default:; //case END_ADDR:
		}
	}
	return false;
}

// Returns NULL only if parsing should be aborted.
// Declared noinline since float values aren't used
// frequently and otherwise binhack_calc_size/binhack_render
// waste a whole register storing the address of errno
static TH_NOINLINE const char* consume_float_value(const char *const expr, patch_val_t *const val)
{
	char* expr_next;
	errno = 0;
	double result = _strtod_l(expr, &expr_next, lc_neutral.locale);
	if (expr == expr_next) {
		// Not actually a floating-point number, keep going though
		val->type = PVT_NONE;
		return expr + 1;
	} else if ((result == HUGE_VAL || result == -HUGE_VAL) && errno == ERANGE) {
		log_printf("ERROR: Floating point constant \"%.*s\" out of range!\n", expr_next - expr, expr);
		return NULL;
	}
	switch (expr_next[0] | 0x20) {
		case 'd':
			++expr_next;
		default:
			val->type = PVT_DOUBLE;
			val->d = result;
			return expr_next;
		case 'f':
			val->type = PVT_FLOAT;
			val->f = (float)result;
			return expr_next + 1;
		case 'l':
			val->type = PVT_LONGDOUBLE;
			val->ld = /*(LongDouble80)*/result;
			return expr_next + 1;
	}
}

// TODO: Check if anyone uses single quotes already in code strings
//// Char
//else if (expr[0] == '\'' && (expr_next = (char*)strchr(expr+1, '\''))) {
//  val->type = PVT_SBYTE;
//	if (expr[1] == '\\') {
//		switch (expr[2]) {
//			case '0':  val->sb = '\0'; break;
//			case 'a':  val->sb = '\a'; break;
//			case 'b':  val->sb = '\b'; break;
//			case 'f':  val->sb = '\f'; break;
//			case 'n':  val->sb = '\n'; break;
//			case 'r':  val->sb = '\r'; break;
//			case 't':  val->sb = '\t'; break;
//			case 'v':  val->sb = '\v'; break;
//			case '\\': val->sb = '\\'; break;
//			case '\'': val->sb = '\''; break;
//			case '\"': val->sb = '\"'; break;
//			case '\?': val->sb = '\?'; break;
//			default:   val->sb = expr[2]; break;
//		}
//	} else {
//		val->sb = expr[1];
//	}
//	return expr_next + 1;
//}

// Declared forceinline since the compiler can't
// figure it out otherwise and copy/pasting this
// into two functions would be a pain
static TH_FORCEINLINE const char* check_for_code_string_cast(const char* expr, patch_val_t *const val)
{
	switch (const uint8_t c = expr[0] | 0x20) {
		case 'i': case 'u': case 'f':
			if (expr[1] && expr[2]) {
				switch (const uint32_t temp = *(uint32_t*)expr | TextInt(0x20, 0x00, 0x00, 0x00)) {
					case TextInt('f', '3', '2', ':'):
						val->type = PVT_FLOAT;
						return expr + 4;
					case TextInt('f', '6', '4', ':'):
						val->type = PVT_DOUBLE;
						return expr + 4;
					case TextInt('f', '8', '0', ':'):
						val->type = PVT_LONGDOUBLE;
						return expr + 4;
					case TextInt('u', '1', '6', ':'):
						val->type = PVT_WORD;
						return expr + 4;
					case TextInt('i', '1', '6', ':'):
						val->type = PVT_SWORD;
						return expr + 4;
					case TextInt('u', '3', '2', ':'):
						val->type = PVT_DWORD;
						return expr + 4;
					case TextInt('i', '3', '2', ':'):
						val->type = PVT_SDWORD;
						return expr + 4;
					case TextInt('u', '6', '4', ':'):
						val->type = PVT_QWORD;
						return expr + 4;
					case TextInt('i', '6', '4', ':'):
						val->type = PVT_SQWORD;
						return expr + 4;
					default:
						switch (temp & TextInt(0xFF, 0xFF, 0xFF, '\0')) {
							case TextInt('u', '8', ':'):
								val->type = PVT_BYTE;
								return expr + 3;
							case TextInt('i', '8', ':'):
								val->type = PVT_SBYTE;
								return expr + 3;
						}
				}
			}
		default:
			//log_printf("WARNING: no code string expression size specified, assuming default...\n");
			val->type = PVT_DEFAULT;
			return expr;
	}
}

static inline const char* parse_brackets(const char* str, char c) {
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

size_t code_string_calc_size(const char* code_str) {
	if (!code_str) {
		return 0;
	}
	size_t size = 0;
	patch_val_t val;
	val.type = PVT_NONE;
	while (1) {
		// Parse characters
		switch (uint8_t cur_char = code_str[0]) {
			case '\0': // End of string
				return size;
			case '(': case '{': case '[':
				++code_str;
				if (cur_char == '[') { // Relative patch value
					val.type = PVT_DWORD; // x64 uses 32 bit relative offsets as well
				}
				else { // Expressions
					code_str = check_for_code_string_cast(code_str, &val);
				}
				code_str = parse_brackets(code_str, cur_char);
				break;
			case '<': // Absolute patch value
				code_str = get_patch_value(code_str, &val, NULL, 0);
				break;
			case '?': // Wildcard byte
				++code_str;
				if (code_str[0] != '?') {
					// Not a full wildcard byte, ignore current character
					// and parse the next character from the beginning
					continue;
				}
				// Found a wildcard byte, so just add
				// a byte of size and keep going
				val.type = PVT_BYTE;
				++code_str;
				break;
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
				// Raw byte
				++code_str;
				if (!is_valid_hex(code_str[0])) {
					// Next character doesn't form complete byte, so
					// ignore the current character and parse the
					// next character from the beginning
					continue;
				}
				val.type = PVT_BYTE;
				++code_str;
				break;
			case '+': case '-': // Float
				code_str = consume_float_value(code_str, &val);
				break;
			default: // Skip character
				++code_str;
				continue;
		}

		// Check for errors
		if unexpected(!code_str) {
			// Code string calc size error
			return 0;
		}

		// Add to size
		switch (val.type) {
			case PVT_BYTE: case PVT_SBYTE:
				size += sizeof(int8_t);
				break;
			case PVT_WORD: case PVT_SWORD:
				size += sizeof(int16_t);
				break;
			case PVT_DWORD: case PVT_SDWORD:
				size += sizeof(int32_t);
				break;
			case PVT_QWORD: case PVT_SQWORD:
				size += sizeof(int64_t);
				break;
			case PVT_FLOAT:
				size += sizeof(float);
				break;
			case PVT_DOUBLE:
				size += sizeof(double);
				break;
			case PVT_LONGDOUBLE:
				size += sizeof(LongDouble80);
				break;
			case PVT_STRING:
				size += sizeof(const char*);
				break;
			case PVT_STRING16:
				size += sizeof(const char16_t*);
				break;
			case PVT_STRING32:
				size += sizeof(const char32_t*);
				break;
			case PVT_CODE:
				size += val.code.len * val.code.count;
				break;
			case PVT_NONE:
				break;
			default:
				TH_UNREACHABLE;
		}
	}
}

int code_string_render(uint8_t* output_buffer, uintptr_t target_addr, const char* code_str) {

// Dang compatibility things
#define CodeStringErrorRet		1
#define CodeStringSuccessRet	0

	if (!output_buffer || !code_str) {
		return CodeStringErrorRet;
	}

#define CodeStringRenderError() code_str = NULL;

	patch_val_t val;

	while (1) {
		// Parse characters
		switch (uint8_t cur_char = code_str[0]) {
			case '\0': // End of string
				return CodeStringSuccessRet;
			case '(': case '{': // Expression
				code_str = check_for_code_string_cast(++code_str, &val);
				if (cur_char == '(') { // Raw value
					code_str = eval_expr(code_str, ')', &val.z, NULL, target_addr);
					if unexpected(!code_str) {
						break; // Error
					}
					switch (val.type) {
						case PVT_FLOAT:
							val.f = (float)val.z;
							break;
						case PVT_DOUBLE:
							val.d = (double)val.z;
							break;
						case PVT_LONGDOUBLE:
							val.ld = /*(LongDouble80)*/val.z;
							break;
					}
					break;
				}
				else { // Dereference
					code_str = eval_expr(code_str, '}', &val.z, NULL, target_addr);
					if unexpected(!code_str) {
						break; // Error
					}
					switch (val.type) {
						case PVT_BYTE:		val.b = *(uint8_t*)val.z; break;
						case PVT_SBYTE:		val.sb = *(int8_t*)val.z; break;
						case PVT_WORD:		val.w = *(uint16_t*)val.z; break;
						case PVT_SWORD:		val.sw = *(int16_t*)val.z; break;
						case PVT_DWORD:		val.i = *(uint32_t*)val.z; break;
						case PVT_SDWORD:	val.si = *(int32_t*)val.z; break;
						case PVT_QWORD:		val.q = *(uint64_t*)val.z; break;
						case PVT_SQWORD:	val.sq = *(int64_t*)val.z; break;
						case PVT_FLOAT:		val.f = *(float*)val.z; break;
						case PVT_DOUBLE:	val.d = *(double*)val.z; break;
						case PVT_LONGDOUBLE:val.ld = *(LongDouble80*)val.z; break;

						// This switch is only used when the type is set via
						// a cast operation, which doesn't include any of
						// the other patch value types.
						default: TH_UNREACHABLE;
					}
					break;
				}
			case '[': case '<': // Patch value
				code_str = get_patch_value(code_str, &val, NULL, target_addr);
				break;
			case '?': // Wildcard byte
				++code_str;
				if (code_str[0] != '?') {
					// Not a full wildcard byte, ignore current character
					// and parse the next character from the beginning
					continue;
				}
				/*if (!can_deref_target) {
					// Please just don't use an invalid address for
					// target_addr, it makes this hard to implement
					CodeStringRenderError();
					break;
				}*/
				// Found a wildcard byte, so read the contents
				// of the appropriate address into the buffer.
				val.type = PVT_BYTE;
				val.b = *(unsigned char*)target_addr;
				++code_str;
				break;
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
			{ // Raw byte
				++code_str;
				uint8_t low_nibble = code_str[0] - '0';
				if (low_nibble >= 10) {
					low_nibble &= 0xDF;
					low_nibble -= 17;
					if (low_nibble > 6) {
						// Next character doesn't form complete byte, so
						// ignore the current character and parse the
						// next character from the beginning
						continue;
					}
					low_nibble += 10;
				}
				cur_char -= '0'; // high_nibble
				if (cur_char >= 10) {
					cur_char &= 0xDF;
					cur_char -= 7;
				}
				val.b = cur_char << 4 | low_nibble;
				val.type = PVT_BYTE;
				++code_str;
				break;
			}
			case '+': case '-': // Float
				code_str = consume_float_value(code_str, &val);
				break;
			default: // Skip character
				++code_str;
				continue;
		}

		// Check for errors
		if unexpected(!code_str) {
			log_printf("Code string render error!\n");
			return CodeStringErrorRet;
		}

#ifdef EnableCodeStringLogging
#define CodeStringLogging(...) log_printf(__VA_ARGS__)
#else
// Using __noop makes the compiler check the validity of
// the macro contents without actually compiling them.
#define CodeStringLogging(...) TH_EVAL_NOOP(__VA_ARGS__)
#endif

		// Render bytes
		switch (val.type) {
			case PVT_BYTE:
				*(uint8_t*)output_buffer = val.b;
				CodeStringLogging("Code string rendered: %02hhX at %p\n", *(uint8_t*)output_buffer, target_addr);
				output_buffer += sizeof(uint8_t);
				target_addr += sizeof(uint8_t);
				break;
			case PVT_SBYTE:
				*(int8_t*)output_buffer = val.sb;
				CodeStringLogging("Code string rendered: %02hhX at %p\n", *(int8_t*)output_buffer, target_addr);
				output_buffer += sizeof(int8_t);
				target_addr += sizeof(int8_t);
				break;
			case PVT_WORD:
				*(uint16_t*)output_buffer = val.w;
				CodeStringLogging("Code string rendered: %04hX at %p\n", *(uint16_t*)output_buffer, target_addr);
				output_buffer += sizeof(uint16_t);
				target_addr += sizeof(uint16_t);
				break;
			case PVT_SWORD:
				*(int16_t*)output_buffer = val.sw;
				CodeStringLogging("Code string rendered: %04hX at %p\n", *(int16_t*)output_buffer, target_addr);
				output_buffer += sizeof(int16_t);
				target_addr += sizeof(int16_t);
				break;
			case PVT_DWORD:
				*(uint32_t*)output_buffer = val.i;
				CodeStringLogging("Code string rendered: %08X at %p\n", *(uint32_t*)output_buffer, target_addr);
				output_buffer += sizeof(uint32_t);
				target_addr += sizeof(uint32_t);
				break;
			case PVT_SDWORD:
				*(int32_t*)output_buffer = val.si;
				CodeStringLogging("Code string rendered: %08X at %p\n", *(int32_t*)output_buffer, target_addr);
				output_buffer += sizeof(int32_t);
				target_addr += sizeof(int32_t);
				break;
			case PVT_QWORD:
				*(uint64_t*)output_buffer = val.q;
				CodeStringLogging("Code string rendered: %016X at %p\n", *(uint64_t*)output_buffer, target_addr);
				output_buffer += sizeof(uint64_t);
				target_addr += sizeof(uint64_t);
				break;
			case PVT_SQWORD:
				*(int64_t*)output_buffer = val.sq;
				CodeStringLogging("Code string rendered: %016X at %p\n", *(int64_t*)output_buffer, target_addr);
				output_buffer += sizeof(int64_t);
				target_addr += sizeof(int64_t);
				break;
			case PVT_FLOAT:
				*(float*)output_buffer = val.f;
				CodeStringLogging("Code string rendered: %X at %p\n", *(uint32_t*)output_buffer, target_addr);
				output_buffer += sizeof(float);
				target_addr += sizeof(float);
				break;
			case PVT_DOUBLE:
				*(double*)output_buffer = val.d;
				CodeStringLogging("Code string rendered: %llX at %p\n", *(uint64_t*)output_buffer, target_addr);
				output_buffer += sizeof(double);
				target_addr += sizeof(double);
				break;
			case PVT_LONGDOUBLE:
				*(LongDouble80*)output_buffer = val.ld;
				CodeStringLogging("Code string rendered: %llX%hX at %p\n", *(uint64_t*)output_buffer, *(uint16_t*)(output_buffer + 8), target_addr);
				output_buffer += sizeof(LongDouble80);
				target_addr += sizeof(LongDouble80);
				break;
			case PVT_STRING:
				*(const char**)output_buffer = val.str.ptr;
				CodeStringLogging("Code string rendered: %zX at %p\n", (size_t)*(const char**)output_buffer, target_addr);
				output_buffer += sizeof(const char*);
				target_addr += sizeof(const char*);
				break;
			case PVT_STRING16:
				*(const char16_t**)output_buffer = val.str16.ptr;
				CodeStringLogging("Code string rendered: %zX at %p\n", (size_t)*(const char16_t**)output_buffer, target_addr);
				output_buffer += sizeof(const char16_t*);
				target_addr += sizeof(const char16_t*);
				break;
			case PVT_STRING32:
				*(const char32_t**)output_buffer = val.str32.ptr;
				CodeStringLogging("Code string rendered: %zX at %p\n", (size_t)*(const char32_t**)output_buffer, target_addr);
				output_buffer += sizeof(const char32_t*);
				target_addr += sizeof(const char32_t*);
				break;
			case PVT_CODE: {
				while (val.code.count--) {
					if unexpected(code_string_render(output_buffer, target_addr, val.code.ptr)) {
						return CodeStringErrorRet;
					}
					output_buffer += val.code.len;
					target_addr += val.code.len;
				}
				break;
			}
			case PVT_NONE:
				break;
			default:
				TH_UNREACHABLE;
		}
	}
}

bool binhack_from_json(const char *name, json_t *in, binhack_t *out)
{
	if (!json_is_object(in)) {
		log_printf("binhack %s: not an object\n", name);
		return false;
	}

	if (json_object_get_eval_bool_default(in, "ignore", false, JEVAL_DEFAULT) ||
		!json_object_get_eval_bool_default(in, "enable", true, JEVAL_DEFAULT)) {
		log_printf("binhack %s: ignored\n", name);
		return false;
	}

	const char *code = json_object_get_string(in, "code");
	if (!code) {
		// Ignore binhacks with missing fields
		// It usually means the binhack doesn't apply for this game or game version.
		return false;
	}

	hackpoint_addr_t *addrs = hackpoint_addrs_from_json(json_object_get(in, "addr"));
	if (!addrs) {
		// Ignore binhacks with no valid addresses
		return false;
	}

	out->name = strdup(name);
	out->title = json_object_get_string_copy(in, "title");
	out->code = strdup(code);
	out->expected = json_object_get_string_copy(in, "expected");
	out->addr = addrs;

	return true;
}

// Returns the number of all individual instances of binary hacks in [binhacks].
static size_t binhacks_total_count(const binhack_t *binhacks, size_t binhacks_count)
{
	size_t ret = 0;
	for (size_t i = 0; i < binhacks_count; i++) {
		for (size_t j = 0; binhacks[i].addr[j].type != END_ADDR; j++) {
			ret++;
		}
	}
	return ret;
}

size_t binhacks_apply(const binhack_t *binhacks, size_t binhacks_count, HMODULE hMod)
{
	if (!binhacks_count) {
		log_print("No binary hacks to apply.\n");
		return 0;
	}

	size_t failed = binhacks_total_count(binhacks, binhacks_count);

	log_print(
		"------------------------\n"
		"Applying binary hacks...\n"
		"------------------------"
	);

	size_t current_asm_buf_size = BINHACK_BUFSIZE_MIN;
	uint8_t* asm_buf = (uint8_t*)malloc(BINHACK_BUFSIZE_MIN);
	size_t current_exp_buf_size = BINHACK_BUFSIZE_MIN;
	uint8_t* exp_buf = (uint8_t*)malloc(BINHACK_BUFSIZE_MIN);

	for(size_t i = 0; i < binhacks_count; i++) {
		const binhack_t *const cur = &binhacks[i];

		if (cur->title) {
			log_printf("\n(%2d/%2d) %s (%s)... ", i + 1, binhacks_count, cur->title, cur->name);
		} else {
			log_printf("\n(%2d/%2d) %s... ", i + 1, binhacks_count, cur->name);
		}
		
		// calculated byte size of the hack
		const size_t asm_size = code_string_calc_size(cur->code);
		if(!asm_size) {
			log_print("invalid code string size, skipping...\n");
			continue;
		}
		if (asm_size > current_asm_buf_size) {
			if (void* temp = realloc(asm_buf, asm_size)) {
				asm_buf = (uint8_t*)temp;
				current_asm_buf_size = asm_size;
			}
			else {
				log_printf("buffer reallocation failure (%zu bytes), skipping...\n", asm_size);
				continue;
			}
		}

		bool use_expected;
		if (const size_t exp_size = code_string_calc_size(cur->expected);
			!exp_size) {
			use_expected = false;
		}
		else if (exp_size != asm_size) {
			log_printf("different sizes for expected and new code (%zu != %zu), skipping verification... ", exp_size, asm_size);
			use_expected = false;
		}
		else {
			if (exp_size > current_exp_buf_size) {
				if (void* temp = realloc(exp_buf, exp_size)) {
					exp_buf = (uint8_t*)temp;
					current_exp_buf_size = exp_size;
				}
				else {
					log_printf("buffer reallocation failure (%zu bytes), skipping...\n", exp_size);
					continue;
				}
			}
			use_expected = true;
		}
		
		uintptr_t addr;
		for (hackpoint_addr_t* cur_addr = cur->addr;
			 eval_hackpoint_addr(cur_addr, &addr, hMod);
			 ++cur_addr) {

			if (!addr) {
				// NULL_ADDR
				continue;
			}

			log_printf("\nat 0x%p... ", addr);

			if(code_string_render(asm_buf, addr, cur->code)) {
				log_print("invalid code string, skipping...");
				continue;
			}
			if (use_expected && code_string_render(exp_buf, addr, cur->expected)) {
				log_print("invalid expected string, skipping verification... ");
				use_expected = false;
			}
			if(!(cur_addr->binhack_source = (uint8_t*)PatchRegionCopySrc((void*)addr, use_expected ? exp_buf : NULL, asm_buf, NULL, asm_size))) {
				log_print("expected bytes not matched, skipping... ");
				continue;
			}
			log_print("OK");
			--failed;
		}
	}
	free(asm_buf);
	free(exp_buf);
	log_print("\n");
	return failed;
}

bool codecave_from_json(const char *name, json_t *in, codecave_t *out) {
	// Default properties
	size_t size_val = 0;
	const char* code = NULL;
	bool export_val = false;
	CodecaveAccessType access_val = EXECUTE_READWRITE;
	size_t fill_val = 0;

	if (json_is_object(in)) {
		if (json_object_get_eval_bool_default(in, "ignore", false, JEVAL_DEFAULT) ||
			!json_object_get_eval_bool_default(in, "enable", true, JEVAL_DEFAULT)) {
			log_printf("codecave %s: ignored\n", name);
			return false;
		}

		switch (json_object_get_eval_int(in, "size", &size_val, JEVAL_STRICT)) {
			default:
				log_printf("ERROR: invalid json type for size of codecave %s, must be 32-bit integer or string\n", name);
				return false;
			case JEVAL_SUCCESS: {
				if (!size_val) {
					log_printf("codecave %s with size 0 ignored\n", name);
					return false;
				}
				size_t count_val;
				switch (json_object_get_eval_int(in, "count", &count_val, JEVAL_STRICT)) {
					default:
						log_printf("ERROR: invalid json type specified for count of codecave %s, must be 32-bit integer or string\n", name);
						return false;
					case JEVAL_SUCCESS:
						if (!count_val) {
							log_printf("codecave %s with count 0 ignored\n", name);
							return false;
						}
						size_val *= count_val;
					case JEVAL_NULL_PTR:
						break;
				}
				break;
			}
			case JEVAL_NULL_PTR:
				break;
		}

		code = json_object_get_string(in, "code");

		(void)json_object_get_eval_bool(in, "export", &export_val, JEVAL_DEFAULT);

		if (const char* access = json_object_get_string(in, "access")) {
			uint8_t temp = 0;
			while (char c = *access++ & 0xDF) {
				temp |= (c == 'R');
				temp |= (c == 'W') << 1;
				temp |= ((c == 'E') | (c == 'X')) << 2;
			}
			switch (temp) {
				case 0: /*NOACCESS*/
					log_printf("codecave %s: why would you set a codecave to no access? skipping instead...\n", name);
					return false;
				case 1: /*READONLY*/
					access_val = (CodecaveAccessType)(temp - 1);
					break;
				case 2: /*WRITE*/
					log_printf("codecave %s: write-only codecaves can't be set, using read-write instead...\n", name);
					[[fallthrough]];
				case 3: /*READWRITE*/ case 4: /*EXECUTE*/ case 5: /*EXECUTE-READ*/
					access_val = (CodecaveAccessType)(temp - 2);
					break;
				case 6: /*EXECUTE-WRITE*/
					log_printf("codecave %s: execute-write codecaves can't be set, using execute-read-write instead...\n", name);
					[[fallthrough]];
				case 7: /*EXECUTE-READWRITE*/
					access_val = (CodecaveAccessType)(temp - 3);
					break;
			}
			if (export_val && access_val != EXECUTE && access_val != EXECUTE_READ) {
				log_printf("codecave %s: export can only be applied to execute or execute-read codecaves\n", name);
				export_val = false;
			}
		}
		else {
			if ((code && !size_val) || export_val) {
				access_val = EXECUTE;
			}
			else if (size_val && !code) {
				access_val = READWRITE;
			}
			//else {
			//    access_val = EXECUTE_READWRITE;
			//}
		}

		switch (json_object_get_eval_int(in, "fill", &fill_val, JEVAL_STRICT)) {
			default:
				log_printf("ERROR: invalid json type specified for fill value of codecave %s, must be 32-bit integer or string\n", name);
				return false;
			case JEVAL_SUCCESS:
			case JEVAL_NULL_PTR:
				break;
		}
		
	}
	else if (json_is_string(in)) {
		// size_val = 0;
		code = json_string_value(in);
		// export_val = false;
		// access_val = EXECUTE_READWRITE;
		// fill_val = 0;
	}
	else if (json_is_integer(in)) {
		size_val = (size_t)json_integer_value(in);
		// code = NULL;
		// export_val = false;
		access_val = READWRITE;
		// fill_val = 0;
	}
	else {
		// Don't print an error, this can be used for comments
		return false;
	}

	// Validate codecave size early
	if (code) {
		DisableCodecaveNotFoundWarning(true);
		const size_t code_size = code_string_calc_size(code);
		DisableCodecaveNotFoundWarning(false);
		if (!code_size && !size_val) {
			log_printf("codecave %s with size 0 ignored\n", name);
			return false;
		}
		if (code_size > size_val) {
			size_val = code_size;
		}
		code = strdup(code);
	}
	else if (!size_val) {
		log_printf("codecave %s without \"code\" or \"size\" ignored\n", name);
		return false;
	}

	out->code = code;
	out->name = strdup_cat("codecave:", name);
	out->access_type = access_val;
	out->size = size_val;
	out->fill = (uint8_t)fill_val;
	out->export_codecave = export_val;

	return true;
}

size_t codecaves_apply(const codecave_t *codecaves, size_t codecaves_count) {
	if (codecaves_count == 0) {
		return 0;
	}

	log_print(
		"------------------------\n"
		"Applying codecaves...\n"
		"------------------------\n"
	);

	static constexpr size_t codecave_sep_size_min = 1;

	//One size for each valid access flag combo
	size_t codecaves_alloc_size[5] = { 0, 0, 0, 0, 0 };

	size_t codecave_export_count = 0;

	VLA(size_t, codecaves_full_size, codecaves_count * sizeof(size_t));

	// First pass: calc the complete codecave size
	for (size_t i = 0; i < codecaves_count; ++i) {
		if (codecaves[i].export_codecave) {
			++codecave_export_count;
		}
		const size_t size = codecaves[i].size + codecave_sep_size_min;
		codecaves_full_size[i] = AlignUpToMultipleOf2(size, 16);
		codecaves_alloc_size[codecaves[i].access_type] += codecaves_full_size[i];
	}

	uint8_t* codecave_buf[5] = { NULL, NULL, NULL, NULL, NULL };
	for (int i = 0; i < 5; ++i) {
		if (codecaves_alloc_size[i] > 0) {
			codecave_buf[i] = (uint8_t*)VirtualAlloc(NULL, codecaves_alloc_size[i], MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			if (codecave_buf[i]) {
				/*
				*  TODO: Profile whether it's faster to memset the whole block
				*  here or just the extra padding required after each codecave.
				*  Apply the same thing to breakpoint sourcecaves.
				*/
				//memset(codecave_buf[i], 0xCC, codecaves_total_size[i]);
			}
			else {
				//Should probably put an abort error here
			}
		}
	}

	// Second pass: Gather the addresses, so that codecaves can refer to each other.
	uint8_t* current_cave[5];
	for (int i = 0; i < 5; ++i) {
		current_cave[i] = codecave_buf[i];
	}

	exported_func_t* codecaves_export_table;
	if (codecave_export_count > 0) {
		codecaves_export_table = (exported_func_t*)malloc((codecave_export_count + 1) * sizeof(exported_func_t));
		codecaves_export_table[codecave_export_count].name = NULL;
		codecaves_export_table[codecave_export_count].func = NULL;
	}
	size_t export_index = 0;
	
	for (size_t i = 0; i < codecaves_count; ++i) {
		const CodecaveAccessType access = codecaves[i].access_type;

		log_printf("Recording codecave: \"%s\" at %p\n", codecaves[i].name, (uintptr_t)current_cave[access]);
		func_add(codecaves[i].name, (uintptr_t)current_cave[access]);

		if (codecaves[i].export_codecave) {
			codecaves_export_table[export_index].name = codecaves[i].name;
			codecaves_export_table[export_index].func = (uintptr_t)current_cave[access];
			++export_index;
		}

		current_cave[access] += codecaves_full_size[i];
	}

	// Third pass: Write all of the code
	for (int i = 0; i < 5; ++i) {
		current_cave[i] = codecave_buf[i];
	}

	for (size_t i = 0; i < codecaves_count; ++i) {
		const char* code = codecaves[i].code;
		const CodecaveAccessType access = codecaves[i].access_type;
		if (codecaves[i].fill) {
			// VirtualAlloc zeroes memory, so don't bother when fill is 0
			memset(current_cave[access], codecaves[i].fill, codecaves[i].size);
		}
		if (code) {
			code_string_render(current_cave[access], (uintptr_t)current_cave[access], code);
		}

		current_cave[access] += codecaves[i].size;
		for (size_t j = codecaves[i].size; j < codecaves_full_size[i]; ++j) {
			*(current_cave[access]++) = 0xCC; // x86 INT3
		}
	}

	if (codecave_export_count > 0) {
		patch_func_init(codecaves_export_table);
		free(codecaves_export_table);
	}
	VLA_FREE(codecaves_full_size);

	static constexpr DWORD page_access_type_array[5] = { PAGE_READONLY, PAGE_READWRITE, PAGE_EXECUTE, PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE };
	DWORD idgaf;

	for (int i = 0; i < 5; ++i) {
		if (codecave_buf[i]) {
			VirtualProtect(codecave_buf[i], codecaves_alloc_size[i], page_access_type_array[i], &idgaf);
		}
	}
	return 0;
}
