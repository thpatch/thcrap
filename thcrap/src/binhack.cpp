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
			"\n", func_name, PROJECT_NAME_SHORT()
		);
	}
	return retval;
}

int is_valid_hex(char c)
{
	return
		('0' <= c && c <= '9') ||
		('A' <= c && c <= 'F') ||
		('a' <= c && c <= 'f');
}

enum value_type_t {
	VT_NONE = 0,
	VT_BYTE = 1, // sizeof(char)
	VT_FLOAT = 4, // sizeof(float)
	VT_DOUBLE = 8 // sizeof(double)
};

struct value_t {
	value_type_t type = VT_NONE;
	union {
		unsigned char b;
		float f;
		double d;
	};

	size_t size() {
		return (size_t)type;
	}
};

// Returns false only if parsing should be aborted.
bool consume_value(value_t &val, const char** str)
{
	assert(str);

	const char *c = *str;

	// Double / float
	if(*c == '+' || *c == '-') {
		if(!lc_neutral) {
			lc_neutral = _create_locale(LC_NUMERIC, "C");
		}
		char *endptr;

		errno = 0;
		double result = _strtod_l(*str, &endptr, lc_neutral);
		if(errno == ERANGE && (result == HUGE_VAL || result == -HUGE_VAL)) {
			auto val_len = (endptr - *str);
			log_printf(
				"ERROR: Floating point constant \"%.*s\" out of range!\n",
				val_len, str
			);
			return false;
		} else if(endptr == *str) {
			// Not actually a floating-point number, keep going though
			*str += 1;
			return true;
		}
		if(*endptr == 'f') {
			val.type = VT_FLOAT;
			val.f = (float)result;
			endptr++;
		} else {
			val.type = VT_DOUBLE;
			val.d = result;
		}
		if(*endptr != ' ' && *endptr != '\0') {
			val.type = VT_NONE;
			*str += 1;
		} else {
			*str = endptr;
		}
	}
	// Byte
	else if(is_valid_hex(c[0]) && is_valid_hex(c[1])) {
		char conv[3];
		conv[2] = 0;
		memcpy(conv, *str, 2);
		val.type = VT_BYTE;
		val.b = (unsigned char)strtol(conv, nullptr, 16);
		*str += 2;
	}
	// Nothing, keep going
	else {
		*str += 1;
	}
	return true;
}

size_t binhack_calc_size(const char *binhack_str)
{
	size_t size = 0;
	const char *c = binhack_str;
	const char *fs = NULL; // function start
	if(!binhack_str) {
		return 0;
	}
	while(*c) {
		if(*c == '[' || *c == '<') {
			if(fs) {
				log_printf("ERROR: Nested function pointers near %s!\n", c);
				return 0;
			}
			fs = c + 1;
			c++;
		} else if(fs) {
			if((*c == ']' || *c == '>')) {
				size += sizeof(void*);
				fs = nullptr;
			}
			c++;
		} else {
			value_t val;
			if(!consume_value(val, &c)) {
				return 0;
			}
			size += val.size();
		}
	}
	if(fs) {
		log_printf("ERROR: Function name '%s' not terminated...\n", fs);
		size = 0;
	}
	return size;
}

bool binhack_from_json(const char *name, json_t *in, binhack_t *out)
{
	if (!json_is_object(in)) {
		log_printf("binhack %s: not an object\n", name);
		return false;
	}

	json_t *addr = json_object_get(in, "addr");
	const char *title = json_object_get_string(in, "title");;
	const char *code = json_object_get_string(in, "code");
	const char *expected = json_object_get_string(in, "expected");
	bool ignore = json_is_true(json_object_get(in, "ignore"));

	if (ignore) {
		log_printf("binhack %s: ignored\n", name);
		return false;
	}
	if (!code || json_flex_array_size(addr) == 0) {
		// Ignore binhacks with missing fields
		// It usually means the binhack doesn't apply for this game or game version.
		return false;
	}

	out->name = strdup(name);
	out->title = strdup(title);
	out->code = strdup(code);
	out->expected = strdup(expected);
	out->addr = new char*[(json_flex_array_size(addr) + 1)];

	size_t i;
	json_t *it;
	json_flex_array_foreach(addr, i, it) {
		if (json_is_string(it)) {
			out->addr[i] = strdup(json_string_value(it));
		}
	}
	out->addr[i] = nullptr;

	return true;
}

int binhack_render(BYTE *binhack_buf, size_t target_addr, const char *binhack_str)
{
	const char *c = binhack_str;
	const char *fs = NULL; // function start
	size_t written = 0;
	int func_rel = 0; // Relative function pointer flag
	int ret = 0;

	if(!binhack_buf || !binhack_str) {
		return -1;
	}

	while(*c) {
		if(*c == '[' || *c == '<') {
			if(fs) {
				log_printf("ERROR: Nested function pointers near %s!\n", c);
				return 0;
			}
			func_rel = (*c == '[');
			fs = c + 1;
			c++;
		} else if(fs && (*c == ']' || *c == '>')) {
			VLA(char, function, (c - fs) + 1);
			defer({ VLA_FREE(function); });
			size_t fp = 0;

			strncpy(function, fs, c - fs);
			function[c - fs] = 0;

			fp = (size_t)func_get(function);
			if(fp) {
				if(func_rel) {
					fp -= target_addr + written + sizeof(void*);
				}
				memcpy(binhack_buf, &fp, sizeof(void*));
				binhack_buf += sizeof(void*);
				written += sizeof(void*);
			} else {
				return hackpoints_error_function_not_found(function, 2);
			}
			fs = NULL;
			if(ret) {
				break;
			}
			c++;
		} else if(fs) {
			c++;
		} else {
			value_t val;
			if(!consume_value(val, &c)) {
				return 1;
			}
			const char *copy_ptr = nullptr;
			switch(val.type) {
			case VT_BYTE:   copy_ptr = (const char *)&val.b; break;
			case VT_FLOAT:  copy_ptr = (const char *)&val.f; break;
			case VT_DOUBLE: copy_ptr = (const char *)&val.d; break;
			default:        break; // -Wswitch...
			}
			if(copy_ptr) {
				binhack_buf = (BYTE *)memcpy_advance_dst(
					(char *)binhack_buf, copy_ptr, val.size()
				);
				written += val.size();
			}
		}
	}
	if(fs) {
		log_printf("ERROR: Function name '%s' not terminated...\n", fs);
		ret = 1;
	}
	return ret;
}

// Returns the number of all individual instances of binary hacks in [binhacks].
static size_t binhacks_total_count(const binhack_t *binhacks, size_t binhacks_count)
{
	int ret = 0;
	for (size_t i = 0; i < binhacks_count; i++) {
		for (size_t j = 0; binhacks[i].addr[j]; j++) {
			ret++;
		}
	}
	return ret;
}

int binhacks_apply(const binhack_t *binhacks, size_t binhacks_count, HMODULE hMod)
{
	size_t c = 0;
	size_t binhacks_total = binhacks_total_count(binhacks, binhacks_count);
	int failed = binhacks_total;

	if(!binhacks_count) {
		log_printf("No binary hacks to apply.\n");
		return 0;
	}

	log_printf("Applying binary hacks...\n");
	log_printf("------------------------\n");

	for(size_t i = 0; i < binhacks_count; i++) {
		const binhack_t& cur = binhacks[i];

		// calculated byte size of the hack
		size_t asm_size = binhack_calc_size(cur.code);
		size_t exp_size = binhack_calc_size(cur.expected);
		if(!asm_size) {
			continue;
		}

		VLA(BYTE, asm_buf, asm_size);
		VLA(BYTE, exp_buf, exp_size);
		for(size_t j = 0; cur.addr[j]; j++) {
			auto addr = str_address_value(cur.addr[j], hMod, NULL);
			if(!addr) {
				continue;
			}
			log_printf("(%2d/%2d) 0x%p ", ++c, binhacks_total, addr);
			if(cur.title) {
				log_printf("%s (%s)... ", cur.title, cur.name);
			} else {
				log_printf("%s...", cur.name);
			}
			if(binhack_render(asm_buf, addr, cur.code)) {
				continue;
			}
			if(exp_size > 0 && exp_size != asm_size) {
				log_printf("different sizes for expected and new code, skipping verification... ");
				exp_size = 0;
			} else if(binhack_render(exp_buf, addr, cur.expected)) {
				exp_size = 0;
			}
			if(PatchRegion((void*)addr, exp_size ? exp_buf : NULL, asm_buf, asm_size)) {
				log_printf("OK\n");
				failed--;
			} else {
				log_printf("expected bytes not matched, skipping...\n");
			}
		}
		VLA_FREE(asm_buf);
		VLA_FREE(exp_buf);
	}
	log_printf("------------------------\n");
	return failed;
}

int codecaves_apply(const codecave_t *codecaves, size_t codecaves_count, DWORD protection) {
	if (codecaves_count == 0) {
		return 0;
	}

	log_printf("Applying codecaves...\n");
	log_printf("------------------------\n");

	const size_t codecave_sep_size = 3;
	size_t codecaves_total_size = 0;

	// First pass: calc the complete codecave size
	for (size_t i = 0; i < codecaves_count; i++) {
		codecaves_total_size += binhack_calc_size(codecaves[i].code);
		codecaves_total_size += codecave_sep_size;
	}

	BYTE *codecave_buf = (BYTE*)VirtualAlloc(NULL, codecaves_total_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	// Second pass: Gather the addresses, so that codecaves can refer to each other.
	BYTE *current_cave = codecave_buf;

	for (size_t i = 0; i < codecaves_count; i++) {
		const char *codecave_name = codecaves[i].name;
		
		VLA(char, codecave_full_name, strlen(codecave_name) + 10); // strlen("codecave:") = 9
		strcpy(codecave_full_name, "codecave:");
		strcpy(codecave_full_name + 9, codecave_name);
		func_add(codecave_full_name, (size_t)current_cave);

		current_cave += binhack_calc_size(codecaves[i].code) + codecave_sep_size;

		VLA_FREE(codecave_full_name);
	}

	// Third pass: Write all of the code
	current_cave = codecave_buf;

	for (size_t i = 0; i < codecaves_count; i++) {
		const char* code = codecaves[i].code;
		binhack_render(current_cave, (size_t)current_cave, code);

		current_cave += binhack_calc_size(code);
		for (size_t i = 0; i < codecave_sep_size; i++) {
			*current_cave = 0xCC; current_cave++;
		}
	}

	DWORD old_prot;
	if (protection == 0) {
		protection = PAGE_EXECUTE_READ;
	}

	VirtualProtect(codecave_buf, codecaves_total_size, protection, &old_prot);

	return 0;
}

extern "C" __declspec(dllexport) void binhack_mod_exit()
{
	SAFE_CLEANUP(_free_locale, lc_neutral);
}
