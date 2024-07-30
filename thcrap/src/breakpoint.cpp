/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoint handling.
  */

#include "thcrap.h"

/// Constants
/// ---------
#define CALL_REL_OP_LEN 1
#define CALL_ABSPTR_OP_LEN 3
#define CALL_REL_LEN (CALL_REL_OP_LEN + sizeof(int32_t))
#define CALL_ABSPTR_LEN (CALL_ABSPTR_OP_LEN + sizeof(int32_t))
#if TH_X86
#define CALL_OP_LEN CALL_REL_OP_LEN
#define CALL_LEN    CALL_REL_LEN
#define MOV_PTR_LEN 1
#else
#define CALL_OP_LEN CALL_ABSPTR_OP_LEN
#define CALL_LEN    CALL_ABSPTR_LEN
#define MOV_PTR_LEN 2
#endif
#define x86_CALL_NEAR_REL32 0xE8
#define x86_JMP_NEAR_REL32 0xE9
#define x86_CALL_NEAR_ABSPTR TextInt(0xFF, 0x14, 0x25)
#define x86_JMP_NEAR_ABSPTR TextInt(0xFF, 0x24, 0x25)
#define x86_NOP 0x90
#define x86_INT3 0xCC
/// ---------

size_t json_immediate_value(json_t *val, x86_reg_t *regs)
{
	if (val) {
		switch (json_typeof(val)) {
			default:
				TH_UNREACHABLE;
			case JSON_INTEGER:
				return (size_t)json_integer_value(val);
			case JSON_STRING: {
				size_t ret = 0;
				eval_expr(json_string_value(val), '\0', &ret, regs, NULL, NULL);
				return ret;
			}
			case JSON_REAL:
				return (size_t)json_real_value(val);
			case JSON_TRUE:
				return 1;
			case JSON_OBJECT: case JSON_ARRAY:
				log_func_printf("the expression must not be an array or object.\n");
			case JSON_NULL: case JSON_FALSE:;
		}
	}
	return 0;
}

size_t *json_pointer_value(json_t *val, x86_reg_t *regs)
{
	const char *expr = json_string_value(val);
	if (!expr || json_string_length(val) < 3) {
		return NULL;
	}

	// This function returns a pointer to an expression - that means the expression must resolve to something that can be pointed to.
	// We'll accept only 2 kind of expressions:
	// - A dereferencing (for example "[ebp-8]"), where we'll skip the top-level dereferencing. After all, ebp-8 points to [ebp-8].
	// - A register name, without anything else. In that case, we can return a pointer to the register in the x86_reg_t structure.
	size_t *ptr;
	const char *expr_end;

	ptr = (size_t*)reg(regs, expr, &expr_end);
	if (ptr && expr_end[0] == '\0') {
		return ptr;
	}
	else if (expr[0] == '[') {
		expr_end = eval_expr(expr + 1, ']', (size_t*)&ptr, regs, NULL, NULL);
		if (expr_end && expr_end[0] != ']') {
			log_func_printf("Warning: leftover bytes after dereferencing: '%s'\n", expr_end);
		}
		return ptr;
	}
	log_func_printf("Error: called with something other than a register or a dereferencing.\n");
	return NULL;
}

patch_val_t json_typed_value(json_t *val, x86_reg_t *regs, patch_value_type_t type) {
	patch_val_t ret;

	void* value = json_pointer_value(val, regs);
	if unexpected(!value) {
		ret.type = PVT_UNKNOWN;
		return ret;
	}

	switch (ret.type = type) {
		case PVT_STRING: {
			const char* str = *(char**)value;
			ret.str.ptr = str;
			ret.str.len = strlen(str) + 1;
			break;
		}
		case PVT_STRING16: {
			const char16_t* str = *(char16_t**)value;
			ret.str16.ptr = str;
			size_t length = 0;
			while (str[length++]);
			ret.str16.len = length * sizeof(char16_t);
			break;
		}
		case PVT_STRING32: {
			const char32_t* str = *(char32_t**)value;
			ret.str32.ptr = str;
			size_t length = 0;
			while (str[length++]);
			ret.str32.len = length * sizeof(char32_t);
			break;
		}
		case PVT_SBYTE:
			ret.sq = *(int8_t*)value;
			break;
		case PVT_SWORD:
			ret.sq = *(int16_t*)value;
			break;
		case PVT_SDWORD:
			ret.sq = *(int32_t*)value;
			break;
		case PVT_SQWORD:
			ret.sq = *(int64_t*)value;
			break;
		case PVT_BYTE:
			ret.q = *(uint8_t*)value;
			break;
		case PVT_WORD:
			ret.q = *(uint16_t*)value;
			break;
		case PVT_DWORD:
			ret.q = *(uint32_t*)value;
			break;
		case PVT_QWORD:
			ret.q = *(uint64_t*)value;
			break;
		default:
			ret.type = PVT_UNKNOWN;
	}
	return ret;
}

size_t* json_register_pointer(json_t *val, x86_reg_t *regs)
{
	return json_string_length(val) >= 3 ? (size_t*)reg(regs, json_string_value(val), nullptr) : nullptr;
}

size_t* json_object_get_register(json_t *object, x86_reg_t *regs, const char *key)
{
	return json_register_pointer(json_object_get(object, key), regs);
}

size_t* json_object_get_pointer(json_t *object, x86_reg_t *regs, const char *key)
{
	return json_pointer_value(json_object_get(object, key), regs);
}

size_t json_object_get_immediate(json_t *object, x86_reg_t *regs, const char *key)
{
	return json_immediate_value(json_object_get(object, key), regs);
}

patch_val_t json_object_get_typed(json_t *object, x86_reg_t *regs, const char *key, patch_value_type_t type)
{
	return json_typed_value(json_object_get(object, key), regs, type);
}

int breakpoint_cave_exec_flag(json_t *bp_info)
{
	return json_eval_int_default(json_object_get(bp_info, "cave_exec"), 1, JEVAL_DEFAULT);
}

int breakpoint_cave_exec_flag_eval(x86_reg_t* regs, json_t* bp_info) {
	json_t* cave_exec = json_object_get(bp_info, "cave_exec");
	if (cave_exec) {
		return json_immediate_value(cave_exec, regs);
	}
	return 1;
}

bool breakpoint_from_json(const char *name, json_t *in, breakpoint_t *out) {
	if (!json_is_object(in)) {
		log_printf("breakpoint %s: not an object\n", name);
		return false;
	}

	if (hackpoint_ignored(in)) {
		log_printf("breakpoint %s: ignored\n", name);
		return false;
	}

	size_t cavesize = 0;
	switch (json_object_get_eval_int(in, "cavesize", &cavesize, JEVAL_STRICT)) {
		default:
			log_printf("ERROR: invalid json type for cavesize of breakpoint %s, must be 32-bit integer or string\n", name);
			return false;
		case JEVAL_NULL_PTR:
			log_printf("breakpoint %s: no cavesize specified\n", name);
			return false;
		case JEVAL_SUCCESS:
			if (cavesize < CALL_LEN) {
				log_printf("breakpoint %s: cavesize too small to implement breakpoint\n", name);
				return false;
			}
	}

	hackpoint_addr_t* addrs = hackpoint_addrs_from_json(json_object_get(in, "addr"));
	if (!addrs) {
		// Ignore breakpoints no valid addrs
		// It usually means the breakpoint doesn't apply for this game or game version.
		return false;
	}

	size_t state_type = 0;
	switch (json_object_get_eval_int(in, "type", &state_type, JEVAL_STRICT)) {
		default:
			log_printf("ERROR: invalid breakpoint type for breakpoint %s, must be 32-bit integer or string\n", name);
			return false;
		case JEVAL_SUCCESS:
			state_type &= 0b11;
		case JEVAL_NULL_PTR:
			break;
	}

#if TH_X64
	// x64 CPUs aren't guaranteed to support LAHF/SAHF when operating
	// in long mode, so don't allow setting those breakpoint types
	// if that situation is detected.
	state_type |= !CPU_Supports_LMLSAHF();
#endif
	// State bit 1 indicates whether a stack adjustment is required
	state_type <<= 1;

	size_t stack_clear = 0;
	switch (json_object_get_eval_int(in, "stack_clear_size", &stack_clear, JEVAL_STRICT)) {
		default:
			log_printf("ERROR: invalid breakpoint clear size for breakpoint %s, must be 16-bit integer or string\n", name);
			return false;
		case JEVAL_SUCCESS:
			state_type |= stack_clear != 0;
		case JEVAL_NULL_PTR:
			break;
	}

	out->name = strdup(name);
	out->cavesize = cavesize;

	out->expected = NULL;
	if (const char* expected = json_object_get_concat_string_array(in, "expected")) { // Allocates a string that must be freed if non-null
		size_t expected_size = code_string_validate_size(expected);
		if (expected_size == cavesize) {
			out->expected = expected;
		}
		else {
			log_printf("breakpoint %s: different sizes for expected and cavesize (%zu != %zu), verification will be skipped\n", name, expected_size, cavesize);
			free((void*)expected); // Expected won't be used, so it can be freed
		}
	}

	out->json_obj = json_incref(in);
	out->addr = addrs;
	out->stack_adjust = (uint16_t)stack_clear;
	out->state_flags = (uint8_t)state_type;
	out->func = nullptr;

	return true;
}

static inline void TH_FASTCALL cave_fix(uint8_t* sourcecave, uint8_t* bp_addr, size_t sourcecave_size)
{
	/// Return Jump
	/// ------------------
#if TH_X86
	const uint32_t cave_dist = bp_addr - (sourcecave + CALL_LEN);
	sourcecave[sourcecave_size] = x86_JMP_NEAR_REL32;
	*(uint32_t*)&sourcecave[sourcecave_size + CALL_OP_LEN] = cave_dist;
#else
	*(uint32_t*)&sourcecave[sourcecave_size] = x86_JMP_NEAR_ABSPTR;
	add_constpool_raw_pointer((uintptr_t)(bp_addr + CALL_LEN), (uintptr_t)&sourcecave[sourcecave_size + CALL_OP_LEN]);
#endif

	/// Fix relative stuff
	/// ------------------

	// #1: Relative near call / jump at the very beginning
	if(sourcecave[0] == x86_CALL_NEAR_REL32 || sourcecave[0] == x86_JMP_NEAR_REL32) {
#if TH_X86
		uint32_t offset_old = *(uint32_t*)(sourcecave + CALL_REL_OP_LEN);
		uint32_t offset_new = offset_old + bp_addr - sourcecave;

		*(uint32_t*)(sourcecave + CALL_REL_OP_LEN) = offset_new;

		log_printf("fixing rel offset 0x%X to 0x%X... \n", offset_old, offset_new);
#else
		int32_t offset_old = *(int32_t*)(sourcecave + CALL_REL_OP_LEN);
		int32_t offset_new = sourcecave_size - CALL_REL_OP_LEN + CALL_LEN;

		*(int32_t*)(sourcecave + CALL_REL_OP_LEN) = offset_new;

		*(uint32_t*)&sourcecave[sourcecave_size + CALL_LEN] = x86_JMP_NEAR_ABSPTR;
		add_constpool_raw_pointer((uintptr_t)(bp_addr + CALL_REL_LEN + offset_old), (uintptr_t)&sourcecave[sourcecave_size + CALL_LEN + CALL_OP_LEN]);

		log_printf("fixing rel offset 0x%X... \n", offset_old);
#endif
	}
	/// ------------------

}

static bool breakpoint_local_init(
	breakpoint_t& bp
) {

	const char *const key = bp.name;
	// Multi-slot support
	const char *const slot = strchr(key, '#');
	const size_t key_len = slot ? PtrDiffStrlen(slot, key) : strlen(key);

	char* bp_func_name;
	if (strncmp(key, "codecave:", 9) != 0) {
		bp_func_name = strdup_cat("BP_", { key, key_len });
	} else {
		bp_func_name = strdup_size(key, key_len);
	}
	bp.func = (BreakpointFunc_t)func_get(bp_func_name);

	const bool func_found = (bool)bp.func;
	if (!func_found) {
		hackpoints_error_function_not_found(bp_func_name);
	}
	free(bp_func_name);
	return func_found;
}

extern "C" {
	extern const uint8_t bp_entry0, bp_entry0_jsonptr, bp_entry0_funcptr, bp_entry0_caveptr, bp_entry0_end;

	extern const uint8_t bp_entry0s, bp_entry0s_jsonptr, bp_entry0s_funcptr, bp_entry0s_caveptr, bp_entry0s_end, bp_entry0s_retpop;

	extern const uint8_t bp_entry1, bp_entry1_jsonptr, bp_entry1_funcptr, bp_entry1_caveptr, bp_entry1_end;

	extern const uint8_t bp_entry1s, bp_entry1s_jsonptr, bp_entry1s_funcptr, bp_entry1s_caveptr, bp_entry1s_end, bp_entry1s_retpop;

	extern const uint8_t bp_entry2, bp_entry2_jsonptr, bp_entry2_funcptr, bp_entry2_caveptr, bp_entry2_end;

	extern const uint8_t bp_entry2s, bp_entry2s_jsonptr, bp_entry2s_funcptr, bp_entry2s_caveptr, bp_entry2s_end, bp_entry2s_retpop;

	extern const uint8_t bp_entry3, bp_entry3_jsonptr, bp_entry3_funcptr, bp_entry3_caveptr, bp_entry3_end;

	extern const uint8_t bp_entry3s, bp_entry3s_jsonptr, bp_entry3s_funcptr, bp_entry3s_caveptr, bp_entry3s_end, bp_entry3s_retpop;
}

// Calculate all the offsets once and store them for later

struct BreakpointTypeData {
	const uint8_t* entry_addr;
	ptrdiff_t total_size;
	ptrdiff_t json_ptr_offset;
	ptrdiff_t call_offset;
	ptrdiff_t cave_addr_offset;
	ptrdiff_t ret_pop_offset;
};

#if TH_X86
#define JSON_PTR_OFFSET 1
#define FUNC_PTR_OFFSET 1
#define CAVE_PTR_OFFSET 3
#else
#define JSON_PTR_OFFSET 2
#define FUNC_PTR_OFFSET 2
#define CAVE_PTR_OFFSET 2
#endif
#define RET_POP_OFFSET 1

static const BreakpointTypeData breakpoint_types[] = {
	{
		&bp_entry0,
		&bp_entry0_end                       - &bp_entry0,
		&bp_entry0_jsonptr + JSON_PTR_OFFSET - &bp_entry0,
		&bp_entry0_funcptr + FUNC_PTR_OFFSET - &bp_entry0,
		&bp_entry0_caveptr + CAVE_PTR_OFFSET - &bp_entry0,
		0
	},
	{
		&bp_entry0s,
		&bp_entry0s_end                       - &bp_entry0s,
		&bp_entry0s_jsonptr + JSON_PTR_OFFSET - &bp_entry0s,
		&bp_entry0s_funcptr + FUNC_PTR_OFFSET - &bp_entry0s,
		&bp_entry0s_caveptr + CAVE_PTR_OFFSET - &bp_entry0s,
		&bp_entry0s_retpop  + RET_POP_OFFSET  - &bp_entry0s
	},
	{
		&bp_entry1,
		&bp_entry1_end                       - &bp_entry1,
		&bp_entry1_jsonptr + JSON_PTR_OFFSET - &bp_entry1,
		&bp_entry1_funcptr + FUNC_PTR_OFFSET - &bp_entry1,
		&bp_entry1_caveptr + CAVE_PTR_OFFSET - &bp_entry1,
		0
	},
	{
		&bp_entry1s,
		&bp_entry1s_end                       - &bp_entry1s,
		&bp_entry1s_jsonptr + JSON_PTR_OFFSET - &bp_entry1s,
		&bp_entry1s_funcptr + FUNC_PTR_OFFSET - &bp_entry1s,
		&bp_entry1s_caveptr + CAVE_PTR_OFFSET - &bp_entry1s,
		&bp_entry1s_retpop  + RET_POP_OFFSET  - &bp_entry1s
	},
	{
		&bp_entry2,
		&bp_entry2_end                       - &bp_entry2,
		&bp_entry2_jsonptr + JSON_PTR_OFFSET - &bp_entry2,
		&bp_entry2_funcptr + FUNC_PTR_OFFSET - &bp_entry2,
		&bp_entry2_caveptr + CAVE_PTR_OFFSET - &bp_entry2,
		0
	},
	{
		&bp_entry2s,
		&bp_entry2s_end                       - &bp_entry2s,
		&bp_entry2s_jsonptr + JSON_PTR_OFFSET - &bp_entry2s,
		&bp_entry2s_funcptr + FUNC_PTR_OFFSET - &bp_entry2s,
		&bp_entry2s_caveptr + CAVE_PTR_OFFSET - &bp_entry2s,
		&bp_entry2s_retpop  + RET_POP_OFFSET  - &bp_entry2s
	},
	{
		&bp_entry3,
		&bp_entry3_end                       - &bp_entry3,
		&bp_entry3_jsonptr + JSON_PTR_OFFSET - &bp_entry3,
		&bp_entry3_funcptr + FUNC_PTR_OFFSET - &bp_entry3,
		&bp_entry3_caveptr + CAVE_PTR_OFFSET - &bp_entry3,
		0
	},
	{
		&bp_entry3s,
		&bp_entry3s_end                       - &bp_entry3s,
		&bp_entry3s_jsonptr + JSON_PTR_OFFSET - &bp_entry3s,
		&bp_entry3s_funcptr + FUNC_PTR_OFFSET - &bp_entry3s,
		&bp_entry3s_caveptr + CAVE_PTR_OFFSET - &bp_entry3s,
		&bp_entry3s_retpop  + RET_POP_OFFSET  - &bp_entry3s
	}
};

size_t breakpoints_apply(breakpoint_t *breakpoints, size_t bp_count, HMODULE hMod, HackpointMemoryPage page_array[2])
{
	if(!breakpoints || !bp_count) {
		log_print("No breakpoints to set up.\n");
		return 0;
	}

	size_t sourcecaves_total_size = 0;
	size_t callcaves_total_size = 0;
	size_t failed = bp_count;
	size_t total_valid_addrs = 0;
	size_t largest_cavesize = 0;

	VLA(size_t, breakpoint_total_size, bp_count);

	log_print(
		"-------------------------\n"
		"Setting up breakpoints...\n"
		"-------------------------"
	);

	for (size_t i = 0; i < bp_count; ++i) {

		breakpoint_t& cur = breakpoints[i];

		breakpoint_total_size[i] = 0;

		log_printf(
			"\n(%2zu/%2zu) %s..."
			, i + 1, bp_count, cur.name
		);

		if (!breakpoint_local_init(cur)) {
			// Not found message inside function
			continue;
		}

		size_t cavesize = cur.cavesize;

		if (cur.expected) {
			assert(cavesize == code_string_calc_size(cur.expected));
		}

		size_t total_cavesize = AlignUpToMultipleOf2(cavesize + CALL_LEN, 16);

		size_t cur_valid_addrs = 0;

		uintptr_t addr;
		for (hackpoint_addr_t* cur_addr = cur.addr;
			 eval_hackpoint_addr(cur_addr, &addr, hMod);
			 ++cur_addr
		) {

			if (!addr) {
				// NULL_ADDR
				continue;
			}

			log_printf("\nat 0x%p... ", addr);

			if (!VirtualCheckRegion((const void*)addr, cavesize)) {
				cur_addr->type = INVALID_ADDR;
				log_print("not enough source bytes, skipping...");
				continue;
			}
			++cur_valid_addrs;
			log_print("OK");
			sourcecaves_total_size += total_cavesize;
		}

		if (cur_valid_addrs) {
			breakpoint_total_size[i] = total_cavesize;
			if (cavesize > largest_cavesize) largest_cavesize = cavesize;
			total_valid_addrs += cur_valid_addrs;
			callcaves_total_size += breakpoint_types[cur.state_flags].total_size * cur_valid_addrs;
			--failed;
		}
	}

	if (!total_valid_addrs) {
		log_print("No valid breakpoints to render.\n");
		return failed;
	}

	uint8_t *const cave_source = (uint8_t*)VirtualAlloc(0, sourcecaves_total_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	page_array[0].address = cave_source;
	page_array[0].size = sourcecaves_total_size;
	memset(cave_source, x86_INT3, sourcecaves_total_size);

	uint8_t *const cave_call = (uint8_t*)VirtualAlloc(0, callcaves_total_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	page_array[1].address = cave_call;
	page_array[1].size = callcaves_total_size;

	/*
	for (uint8_t *callcave_fill = cave_call, *const callcave_fill_end = cave_call + callcaves_total_size;
		 callcave_fill < callcave_fill_end;
		 callcave_fill += bp_entry_size
	) {
		memcpy(callcave_fill, (uint8_t*)&bp_entry, bp_entry_size);
	}
	*/

	log_printf(
		"\n"
		"-------------------------\n"
		"Rendering breakpoints... (source cave at 0x%p, call cave at 0x%p)\n"
		"-------------------------\n"
		, cave_source, cave_call
	);

	uint8_t* sourcecave_p = cave_source;
	uint8_t* callcave_p = cave_call;

	uint8_t* asm_buf = (uint8_t*)malloc(largest_cavesize * 2);
	uint8_t* exp_buf = asm_buf + largest_cavesize;
	// CALL bp_entry
#if TH_X86
	asm_buf[0] = x86_CALL_NEAR_REL32;
#else
	*(uint32_t*)asm_buf = x86_CALL_NEAR_ABSPTR;
#endif
	if (largest_cavesize > CALL_LEN) {
		memset(asm_buf + CALL_LEN, x86_NOP, largest_cavesize - CALL_LEN);
	}

#define PatchBPEntryInst(bp_entry_instance, bp_entry_offset, type, value) \
{\
	type *const bp_instance_ptr = (type*const)((uintptr_t)(bp_entry_instance) + (size_t)(bp_entry_offset));\
	*bp_instance_ptr = (type)(value);\
}

	for (size_t i = 0; i < bp_count; ++i) {
		if (breakpoint_total_size[i]) {
			const breakpoint_t *const cur = &breakpoints[i];

			const size_t cavesize = cur->cavesize;

			bool use_expected = (cur->expected != NULL);

			size_t cur_valid_addrs = 0;
			uintptr_t addr;
			for (hackpoint_addr_t* cur_addr = cur->addr;
				 eval_hackpoint_addr(cur_addr, &addr, hMod);
				 ++cur_addr
			) {

				if (!addr) {
					// NULL_ADDR
					continue;
				}

				memcpy(callcave_p, breakpoint_types[cur->state_flags].entry_addr, breakpoint_types[cur->state_flags].total_size);

				PatchBPEntryInst(callcave_p, breakpoint_types[cur->state_flags].json_ptr_offset, json_t*, cur->json_obj);
#if TH_X86
				PatchBPEntryInst(callcave_p, breakpoint_types[cur->state_flags].call_offset, uint32_t, (uintptr_t)cur->func - (uintptr_t)bp_instance_ptr - sizeof(void*));
#else
				PatchBPEntryInst(callcave_p, breakpoint_types[cur->state_flags].call_offset, uintptr_t, cur->func);
#endif
				PatchBPEntryInst(callcave_p, breakpoint_types[cur->state_flags].cave_addr_offset, uintptr_t, sourcecave_p);

				if (uint16_t stack_adjust = cur->stack_adjust) {
					PatchBPEntryInst(callcave_p, breakpoint_types[cur->state_flags].ret_pop_offset, uint16_t, stack_adjust);
				}

#if TH_X86
				// CALL bp_entry
				const uint32_t bp_dist = (uintptr_t)callcave_p - (addr + CALL_LEN);
				// Opcode is set earlier and doesn't change
				*(uint32_t*)&asm_buf[CALL_OP_LEN] = bp_dist;
#endif

				/// Cave assembly
				// Copy old code to cave
				if (use_expected && code_string_render(exp_buf, addr, cur->expected, hMod)) {
					log_print("invalid expected string, skipping verification... ");
					use_expected = false;
				}
				if (cur_addr->breakpoint_source = (uint8_t*)PatchRegionCopySrc((void*)addr, (use_expected ? exp_buf : NULL), asm_buf, sourcecave_p, cavesize)) {
					cave_fix(sourcecave_p, (uint8_t*)addr, cavesize);

#if TH_X64
					// CALL callcave
					add_constpool_raw_pointer((uintptr_t)callcave_p, addr + CALL_OP_LEN);
#endif

					sourcecave_p += breakpoint_total_size[i];
				}
				else {
					log_print("expected bytes not matched, skipping...");
					++failed;
				}

				callcave_p += breakpoint_types[cur->state_flags].total_size;
			}
		}
	}
	free(asm_buf);
	VLA_FREE(breakpoint_total_size);

	DWORD idgaf;
	VirtualProtect(cave_source, sourcecaves_total_size, PAGE_EXECUTE, &idgaf);
	VirtualProtect(cave_call, callcaves_total_size, PAGE_EXECUTE, &idgaf);

	return failed;
}
