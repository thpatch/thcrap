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
#include <string.h>

/*
 * Grumble, grumble, C is garbage and will only do string→float conversion
 * using the decimal separator from the current locale, and OF COURSE it never
 * occured to anyone, not even Microsoft, to provide a neutral strtod() that
 * always looks for a decimal point, and so we have to dynamically allocate
 * (and free) The Neutral Locale instead. C is garbage.
 */
// TODO: Is this really necessary? Documentation for C and MSVC say setlocale(LC_ALL, "C") is effectively run at program startup.
_locale_t lc_neutral = nullptr;

int hackpoints_error_function_not_found(const char *func_name, int retval)
{
	if (runconfig_msgbox_invalid_func()) {
		if (log_mboxf("Binary Hack error", MB_OKCANCEL | MB_ICONERROR, "ERROR: function '%s' not found! ", func_name) == IDCANCEL) {
			thcrap_ExitProcess(0);
		}
	} else {
		log_printf("ERROR: function '%s' not found! "
#ifdef _DEBUG
			"(implementation not exported or still missing?)"
#else
			"(outdated or corrupt %s installation, maybe?)"
#endif
			, func_name, PROJECT_NAME_SHORT()
		);
	}
	return retval;
}

// Returns false only if parsing should be aborted.
const char* consume_value(const char *const expr, value_t *const val) {
	char* expr_next;

	// Double / float
	if (expr[0] == '+' || expr[0] == '-') {
		if (!lc_neutral) {
			lc_neutral = _create_locale(LC_NUMERIC, "C");
		}
		errno = 0;
		double result = _strtod_l(expr, &expr_next, lc_neutral);
		if (expr == expr_next) {
			// Not actually a floating-point number, keep going though
			val->type = VT_NONE;
			return expr + 1;
		} else if (errno == ERANGE && (result == HUGE_VAL || result == -HUGE_VAL)) {
			log_printf( "ERROR: Floating point constant \"%.*s\" out of range!\n", expr_next - expr, expr);
			return expr;
		}
		if (expr_next[0] == 'f') {
			*val = (float)result;
			return expr_next + 1;
		} else {
			*val = result;
			return expr_next;
		}
	}
	// TODO: Check if anyone uses single quotes already in code strings
	//// Char
	//else if (expr[0] == '\'' && (expr_next = (char*)strchr(expr+1, '\''))) {
	//	if (expr[1] == '\\') {
	//		switch (expr[2]) {
	//			case '0':  *val = '\0'; break;
	//			case 'a':  *val = '\a'; break;
	//			case 'b':  *val = '\b'; break;
	//			case 'f':  *val = '\f'; break;
	//			case 'n':  *val = '\n'; break;
	//			case 'r':  *val = '\r'; break;
	//			case 't':  *val = '\t'; break;
	//			case 'v':  *val = '\v'; break;
	//			case '\\': *val = '\\'; break;
	//			case '\'': *val = '\''; break;
	//			case '\"': *val = '\"'; break;
	//			case '\?': *val = '\?'; break;
	//			default:   *val = expr[2]; break;
	//		}
	//	} else {
	//		*val = expr[1];
	//	}
	//	return expr_next + 1;
	//}
	// Byte
	else if (is_valid_hex_byte(expr[0], expr[1])) {
		const uint32_t conv = (uint32_t)*(uint16_t*)expr;
		*val = (unsigned char)strtoul((const char*)&conv, nullptr, 16);
		return expr + 2;
	}
	// Nothing, keep going
	else {
		val->type = VT_NONE;
		return expr + 1;
	}
}

static __forceinline const char* check_for_binhack_cast(const char* expr, value_t *const val) {
	switch (expr[0]) {
		case 'i': case 'I':
		case 'u': case 'U':
		case 'f': case 'F':
			if (expr[1] && expr[2]) {
				const uint32_t temp = *(uint32_t*)expr;
				switch (temp) {
					case TextInt('F', '3', '2', ':'):
					case TextInt('f', '3', '2', ':'):
						val->type = VT_FLOAT;
						return expr + 4;
					case TextInt('F', '6', '4', ':'):
					case TextInt('f', '6', '4', ':'):
						val->type = VT_DOUBLE;
						return expr + 4;
					case TextInt('U', '1', '6', ':'):
					case TextInt('u', '1', '6', ':'):
						val->type = VT_WORD;
						return expr + 4;
					case TextInt('I', '1', '6', ':'):
					case TextInt('i', '1', '6', ':'):
						val->type = VT_SWORD;
						return expr + 4;
					case TextInt('U', '3', '2', ':'):
					case TextInt('u', '3', '2', ':'):
						val->type = VT_DWORD;
						return expr + 4;
					case TextInt('I', '3', '2', ':'):
					case TextInt('i', '3', '2', ':'):
						val->type = VT_SDWORD;
						return expr + 4;
					case TextInt('U', '6', '4', ':'):
					case TextInt('u', '6', '4', ':'):
						val->type = VT_QWORD;
						return expr + 4;
					case TextInt('I', '6', '4', ':'):
					case TextInt('i', '6', '4', ':'):
						val->type = VT_SQWORD;
						return expr + 4;
					default:
						switch (temp & 0x00FFFFFF) {
							case TextInt('U', '8', ':'):
							case TextInt('u', '8', ':'):
								val->type = VT_BYTE;
								return expr + 3;
							case TextInt('I', '8', ':'):
							case TextInt('i', '8', ':'):
								val->type = VT_SBYTE;
								return expr + 3;
						}
				}
			}
	}
	//log_printf("WARNING: no binhack expression size specified, assuming dword...\n");
	val->type = VT_DWORD;
	return expr;
}

size_t binhack_calc_size(const char *binhack_str)
{
	if (!binhack_str) {
		return 0;
	}
	size_t size = 0;
	value_t val;
	const char* copy_ptr;
	while (1) {
		switch (binhack_str[0]) {
			case '\0':
				return size;
			case '(':
				binhack_str = check_for_binhack_cast(++binhack_str, &val);
				copy_ptr = parse_brackets(binhack_str, '(');
				if (binhack_str == copy_ptr) {
					//Bracket error
					return 0;
				}
				binhack_str = copy_ptr;
				break;
			case '{':
				binhack_str = check_for_binhack_cast(++binhack_str, &val);
				copy_ptr = parse_brackets(binhack_str, '{');
				if (binhack_str == copy_ptr) {
					//Bracket error
					return 0;
				}
				binhack_str = copy_ptr;
				break;
			case '[':
				val.type = VT_DWORD;
				copy_ptr = parse_brackets(++binhack_str, '[');
				if (binhack_str == copy_ptr) {
					//Bracket error
					return 0;
				}
				binhack_str = copy_ptr;
				break;
			case '<':
				copy_ptr = get_patch_value(binhack_str, &val, NULL, 0);
				if (binhack_str == copy_ptr) {
					return 0;
				}
				binhack_str = copy_ptr;
				break;
			case '?':
				if (binhack_str[1] == '?') {
					// Found a wildcard byte, so just add
					// a byte of size and keep going
					val.type = VT_BYTE;
					binhack_str += 2;
					break;
				}
				[[fallthrough]];
			default:
				copy_ptr = consume_value(binhack_str, &val);
				if (binhack_str == copy_ptr) {
					return 0;
				}
				binhack_str = copy_ptr;
		}
		switch (val.type) {
			case VT_BYTE: case VT_SBYTE:
				size += sizeof(int8_t);
				break;
			case VT_WORD: case VT_SWORD:
				size += sizeof(int16_t);
				break;
			case VT_DWORD: case VT_SDWORD:
				size += sizeof(int32_t);
				break;
			case VT_QWORD: case VT_SQWORD:
				size += sizeof(int64_t);
				break;
			case VT_FLOAT:
				size += sizeof(float);
				break;
			case VT_DOUBLE:
				size += sizeof(double);
				break;
			case VT_STRING:
				size += sizeof(const char*);
				break;
			/*case VT_CODE:
				size += val.size;
				break;*/
		}
	}
}

bool binhack_from_json(const char *name, json_t *in, binhack_t *out)
{
	if (!json_is_object(in)) {
		log_printf("binhack %s: not an object\n", name);
		return false;
	}

	bool ignore = json_object_get_evaluate_bool(in, "ignore");
	if (ignore) {
		log_printf("binhack %s: ignored\n", name);
		return false;
	}

	json_t *addr = json_object_get(in, "addr");
	const char *code = json_object_get_string(in, "code");
	if (!code || json_flex_array_size(addr) == 0) {
		// Ignore binhacks with missing fields
		// It usually means the binhack doesn't apply for this game or game version.
		return false;
	}

	size_t valid_addrs = 0;

	size_t i;
	json_t *it;
	json_flex_array_foreach(addr, i, it) {
		if (json_can_evaluate_int_strict(it)) {
			++valid_addrs;
		}
	}
	if (valid_addrs == 0) {
		return false;
	}

	const char *expected = json_object_get_string(in, "expected");
	const char *title = json_object_get_string(in, "title");

	out->name = strdup(name);
	out->title = strdup(title);
	out->code = strdup(code);
	out->expected = strdup(expected);
	out->addr = new hackpoint_addr_t[valid_addrs + 1];

	json_flex_array_foreach(addr, i, it) {
		if (json_is_string(it)) {
			out->addr[i].str = strdup(json_string_value(it));
			out->addr[i].type = STR_ADDR;
		}
		else if (json_is_integer(it)) {
			out->addr[i].raw = (uint32_t)json_integer_value(it);
			out->addr[i].type = RAW_ADDR;
		}
	}
	out->addr[i].type = END_ADDR;

	return true;
}

int binhack_render(BYTE *binhack_buf, size_t target_addr, const char *binhack_str)
{
	if (!binhack_buf || !binhack_str || !target_addr) {
		return -1;
	}

	value_t val;
	const char* copy_ptr;

	while (1) {
		switch (binhack_str[0]) {
			case '\0':
				return 0;
			case '(':
				binhack_str = check_for_binhack_cast(++binhack_str, &val);
				copy_ptr = eval_expr(binhack_str, ')', &val.i, NULL, target_addr);
				if (binhack_str == copy_ptr) {
					log_printf("Binhack render error!\n");
					return 1;
				}
				binhack_str = copy_ptr;
				if (val.type == VT_FLOAT) {
					val.f = (float)val.i;
				}
				else if (val.type == VT_DOUBLE) {
					val.d = (double)val.i;
				}
				break;
			case '{':
				binhack_str = check_for_binhack_cast(++binhack_str, &val);
				copy_ptr = eval_expr(binhack_str, '}', &val.i, NULL, target_addr);
				if (binhack_str == copy_ptr) {
					log_printf("Binhack render error!\n");
					return 1;
				}
				binhack_str = copy_ptr;
				switch (val.type) {
					case VT_BYTE:	val.b = *(uint8_t*)val.i; break;
					case VT_SBYTE:	val.sb = *(int8_t*)val.i; break;
					case VT_WORD:	val.w = *(uint16_t*)val.i; break;
					case VT_SWORD:	val.sw = *(int16_t*)val.i; break;
					case VT_DWORD:	val.i = *(uint32_t*)val.i; break;
					case VT_SDWORD:	val.si = *(int32_t*)val.i; break;
					case VT_QWORD:  val.q = *(uint64_t*)val.i; break;
					case VT_SQWORD: val.sq = *(int64_t*)val.i; break;
					case VT_FLOAT:	val.f = *(float*)val.i; break;
					case VT_DOUBLE:	val.d = *(double*)val.i; break;
					case VT_STRING:
					//case VT_CODE:
						log_printf("Binhack render error!\n");
						return 1;
				}
				break;
			case '[':
			case '<':
				copy_ptr = get_patch_value(binhack_str, &val, NULL, target_addr);
				if (binhack_str == copy_ptr) {
					log_printf("Binhack render error!\n");
					return 1;
				}
				binhack_str = copy_ptr;
				break;
			case '?':
				if (binhack_str[1] == '?') {
					// Found a wildcard byte, so read the contents
					// of the appropriate address into the buffer.
					//val.b = *(unsigned char*)(target_addr + written);
					val = *(unsigned char*)target_addr;
					binhack_str += 2;
					break;
				}
				[[fallthrough]];
			default:
				copy_ptr = consume_value(binhack_str, &val);
				if (binhack_str == copy_ptr) {
					return 1;
				}
				binhack_str = copy_ptr;
		}
		switch (val.type) {
			case VT_BYTE:
				*(uint8_t*)binhack_buf = val.b;
				//log_printf("Binhack rendered: %02hhX at %p\n", *(uint8_t*)binhack_buf, target_addr);
				binhack_buf += sizeof(uint8_t);
				target_addr += sizeof(uint8_t);
				break;
			case VT_SBYTE:
				*(int8_t*)binhack_buf = val.sb;
				//log_printf("Binhack rendered: %02hhX at %p\n", *(int8_t*)binhack_buf, target_addr);
				binhack_buf += sizeof(int8_t);
				target_addr += sizeof(int8_t);
				break;
			case VT_WORD:
				*(uint16_t*)binhack_buf = val.w;
				//log_printf("Binhack rendered: %04hX at %p\n", *(uint16_t*)binhack_buf, target_addr);
				binhack_buf += sizeof(uint16_t);
				target_addr += sizeof(uint16_t);
				break;
			case VT_SWORD:
				*(int16_t*)binhack_buf = val.sw;
				//log_printf("Binhack rendered: %04hX at %p\n", *(int16_t*)binhack_buf, target_addr);
				binhack_buf += sizeof(int16_t);
				target_addr += sizeof(int16_t);
				break;
			case VT_DWORD:
				*(uint32_t*)binhack_buf = val.i;
				//log_printf("Binhack rendered: %08X at %p\n", *(uint32_t*)binhack_buf, target_addr);
				binhack_buf += sizeof(uint32_t);
				target_addr += sizeof(uint32_t);
				break;
			case VT_SDWORD:
				*(int32_t*)binhack_buf = val.si;
				//log_printf("Binhack rendered: %08X at %p\n", *(int32_t*)binhack_buf, target_addr);
				binhack_buf += sizeof(int32_t);
				target_addr += sizeof(int32_t);
				break;
			case VT_QWORD:
				*(uint64_t*)binhack_buf = val.q;
				//log_printf("Binhack rendered: %016X at %p\n", *(uint64_t*)binhack_buf, target_addr);
				binhack_buf += sizeof(uint64_t);
				target_addr += sizeof(uint64_t);
				break;
			case VT_SQWORD:
				*(int64_t*)binhack_buf = val.sq;
				//log_printf("Binhack rendered: %016X at %p\n", *(int64_t*)binhack_buf, target_addr);
				binhack_buf += sizeof(int64_t);
				target_addr += sizeof(int64_t);
				break;
			case VT_FLOAT:
				*(float*)binhack_buf = val.f;
				//log_printf("Binhack rendered: %X at %p\n", *(uint32_t*)binhack_buf, target_addr);
				binhack_buf += sizeof(float);
				target_addr += sizeof(float);
				break;
			case VT_DOUBLE:
				*(double*)binhack_buf = val.d;
				//log_printf("Binhack rendered: %llX at %p\n", *(uint64_t*)binhack_buf, target_addr);
				binhack_buf += sizeof(double);
				target_addr += sizeof(double);
				break;
			case VT_STRING:
				*(const char**)binhack_buf = val.str;
				//log_printf("Binhack rendered: %X at %p\n", *(uint32_t*)binhack_buf, target_addr);
				binhack_buf += sizeof(const char*);
				target_addr += sizeof(const char*);
				break;
			/*case VT_CODE: {
				if (binhack_render(binhack_buf, target_addr, val.str)) {
					return 1;
				}
				binhack_buf += val.size;
				target_addr += val.size;
				break;
			}*/
		}
	}
}

// Returns the number of all individual instances of binary hacks in [binhacks].
static size_t binhacks_total_count(const binhack_t *binhacks, size_t binhacks_count)
{
	int ret = 0;
	for (size_t i = 0; i < binhacks_count; i++) {
		for (size_t j = 0; binhacks[i].addr[j].type != END_ADDR; j++) {
			ret++;
		}
	}
	return ret;
}

int binhacks_apply(const binhack_t *binhacks, size_t binhacks_count, HMODULE hMod)
{
	if (!binhacks_count) {
		log_printf("No binary hacks to apply.\n");
		return 0;
	}

	size_t binhacks_total = binhacks_total_count(binhacks, binhacks_count);
	int failed = binhacks_total;

	log_printf(
		"------------------------\n"
		"Applying binary hacks...\n"
		"------------------------"
	);

	for(size_t i = 0; i < binhacks_count; i++) {
		const binhack_t& cur = binhacks[i];

		if (cur.title) {
			log_printf("\n(%2d/%2d) %s (%s)... ", i + 1, binhacks_total, cur.title, cur.name);
		} else {
			log_printf("\n(%2d/%2d) %s... ", i + 1, binhacks_total, cur.name);
		}
		
		// calculated byte size of the hack
		size_t asm_size = binhack_calc_size(cur.code);
		if(!asm_size) {
			log_printf("invalid code string size, skipping...\n");
			continue;
		}
		size_t exp_size = binhack_calc_size(cur.expected);
		if (exp_size > 0 && exp_size != asm_size) {
			log_printf("different sizes for expected and new code (%z != %z), skipping verification... ", exp_size, asm_size);
			exp_size = 0;
		}

		VLA(BYTE, asm_buf, asm_size);
		VLA(BYTE, exp_buf, exp_size);
		for(size_t j = 0;; j++) {
			const hackpoint_addr_t *const addr_ref = &cur.addr[j];

			size_t addr = 0;
			if (addr_ref->type == END_ADDR) {
				break;
			}
			else if (addr_ref->type == STR_ADDR) {
				eval_expr(addr_ref->str, '\0', &addr, NULL, (size_t)hMod);
			}
			else if (addr_ref->type == RAW_ADDR) {
				addr = addr_ref->raw;
			}
			if(!addr) {
				continue;
			}

			log_printf("\nat 0x%p... ", addr);

			if(binhack_render(asm_buf, addr, cur.code)) {
				log_printf("invalid code string, skipping...");
				continue;
			}
			if (exp_size > 0 && binhack_render(exp_buf, addr, cur.expected)) {
				log_printf("invalid expected string, skipping verification... ");
				exp_size = 0;
			}
			if(!PatchRegion((void*)addr, exp_size ? exp_buf : NULL, asm_buf, asm_size)) {
				log_printf("expected bytes not matched, skipping... ");
				continue;
			}
			log_printf("OK");
			failed--;
		}
		VLA_FREE(asm_buf);
		VLA_FREE(exp_buf);
	}
	log_printf("\n");
	return failed;
}

bool codecave_from_json(const char *name, json_t *in, codecave_t *out) {
	// Default properties
	size_t size_val = 0;
	const char* code = NULL;
	bool export_val = false;
	CodecaveAccessType access_val = EXECUTE_READWRITE;
	BYTE fill_val = 0;

	if (json_is_object(in)) {
		if (json_object_get_evaluate_bool(in, "ignore")) {
			log_printf("codecave %s: ignored\n", name);
			return false;
		}

		json_t *j_temp = json_object_get(in, "size");
		if (j_temp) {
			if (!json_can_evaluate_int_strict(j_temp)) {
				log_printf("ERROR: invalid json type for size of codecave %s, must be integer or string\n", name);
				return false;
			}
			size_val = json_evaluate_int(j_temp);
			if (!size_val) {
				log_printf("codecave %s with size 0 ignored\n", name);
				return false;
			}
			j_temp = json_object_get(in, "count");
			if (j_temp) {
				if (!json_can_evaluate_int_strict(j_temp)) {
					log_printf("ERROR: invalid json type specified for count of codecave %s, must be integer or string\n", name);
					return false;
				}
				const size_t count_val = json_evaluate_int(j_temp);
				if (!count_val) {
					log_printf("codecave %s with count 0 ignored\n", name);
					return false;
				}
				size_val *= count_val;
			}
		}
		//else {
		//    size_val = 0;
		//}

		code = json_object_get_string(in, "code");
		export_val = json_object_get_evaluate_bool(in, "export");

		const char* access = json_object_get_string(in, "access");
		if (access) {
			BYTE temp = !!strpbrk(access, "rR") + (!!strpbrk(access, "wW") << 1) + (!!strpbrk(access, "eExX") << 2);
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

		j_temp = json_object_get(in, "fill");
		if (j_temp) {
			if (!json_can_evaluate_int_strict(j_temp)) {
				log_printf("ERROR: invalid json type specified for fill value of codecave %s, must be integer or string\n", name);
				return false;
			}
			fill_val = (BYTE)json_evaluate_int(j_temp);
		}
		//else {
		//    fill_val = 0;
		//}
		
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
		const size_t code_size = binhack_calc_size(code);
		DisableCodecaveNotFoundWarning(false);
		if (!code_size && !size_val) {
			log_printf("codecave %s with size 0 ignored\n", name);
			return false;
		}
		if (code_size > size_val) {
			size_val = code_size;
		}
	}
	else if (!size_val) {
		log_printf("codecave %s without \"code\" or \"size\" ignored\n", name);
		return false;
	}

	out->code = strdup(code);
	char *const name_str = strndup("codecave:", strlen(name) + 10);
	strcpy(name_str + 9, name);
	out->name = name_str;
	out->access_type = access_val;
	out->size = size_val;
	out->fill = fill_val;
	out->export_codecave = export_val;

	return true;
}

int __declspec(safebuffers) codecaves_apply(const codecave_t *codecaves, size_t codecaves_count) {
	if (codecaves_count == 0) {
		return 0;
	}

	log_printf(
		"------------------------\n"
		"Applying codecaves...\n"
		"------------------------\n"
	);

	const size_t codecave_sep_size_min = 1;

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
		codecaves_full_size[i] = AlignUpToMultipleOf(size, 16);
		codecaves_alloc_size[codecaves[i].access_type] += codecaves_full_size[i];
	}

	BYTE* codecave_buf[5] = { NULL, NULL, NULL, NULL, NULL };
	for (int i = 0; i < 5; ++i) {
		if (codecaves_alloc_size[i] > 0) {
			codecave_buf[i] = (BYTE*)VirtualAlloc(NULL, codecaves_alloc_size[i], MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
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
	BYTE* current_cave[5];
	for (int i = 0; i < 5; ++i) {
		current_cave[i] = codecave_buf[i];
	}

	exported_func_t* codecaves_export_table;
	if (codecave_export_count > 0) {
		codecaves_export_table = (exported_func_t*)malloc((codecave_export_count + 1) * sizeof(exported_func_t));
		codecaves_export_table[codecave_export_count].name = NULL;
		codecaves_export_table[codecave_export_count].func = 0;
	}
	size_t export_index = 0;
	
	for (size_t i = 0; i < codecaves_count; i++) {
		const CodecaveAccessType access = codecaves[i].access_type;

		log_printf("Recording codecave: \"%s\" at %p\n", codecaves[i].name, (size_t)current_cave[access]);
		func_add(codecaves[i].name, (size_t)current_cave[access]);

		if (codecaves[i].export_codecave) {
			codecaves_export_table[export_index].name = codecaves[i].name;
			codecaves_export_table[export_index].func = (UINT_PTR)current_cave[access];
			++export_index;
		}

		current_cave[access] += codecaves_full_size[i];
	}

	// Third pass: Write all of the code
	for (int i = 0; i < 5; ++i) {
		current_cave[i] = codecave_buf[i];
	}

	for (size_t i = 0; i < codecaves_count; i++) {
		const char* code = codecaves[i].code;
		const CodecaveAccessType access = codecaves[i].access_type;
		if (codecaves[i].fill) {
			// VirtualAlloc zeroes memory, so don't bother when fill is 0
			memset(current_cave[access], codecaves[i].fill, codecaves[i].size);
		}
		if (code) {
			binhack_render(current_cave[access], (size_t)current_cave[access], code);
		}

		current_cave[access] += codecaves[i].size;
		for (size_t j = codecaves[i].size; j < codecaves_full_size[i]; ++j) {
			*(current_cave[access]++) = 0xCC;
		}
	}

	if (codecave_export_count > 0) {
		patch_func_init(codecaves_export_table, codecave_export_count);
		free((void*)codecaves_export_table);
	}
	VLA_FREE(codecaves_full_size);

	const DWORD page_access_type_array[5] = { PAGE_READONLY, PAGE_READWRITE, PAGE_EXECUTE, PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE };
	DWORD idgaf;

	for (int i = 0; i < 5; ++i) {
		if (codecave_buf[i]) {
			VirtualProtect(codecave_buf[i], codecaves_alloc_size[i], page_access_type_array[i], &idgaf);
		}
	}
	return 0;
}

extern "C" __declspec(dllexport) void binhack_mod_exit()
{
	SAFE_CLEANUP(_free_locale, lc_neutral);
}
