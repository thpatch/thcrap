/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoint handling.
  */

#include "thcrap.h"

/// Functions
/// ---------
// Breakpoint hook function, implemented in assembly. A CALL to this function
// is written to every breakpoint's address.
extern "C" void bp_entry(void);

// Performs breakpoint lookup, invocation and stack adjustments. Returns the
// number of bytes the stack has to be moved downwards by breakpoint_entry().
extern "C" size_t breakpoint_process(breakpoint_local_t *bp_local, x86_reg_t *regs);
/// ---------

/// Constants
/// ---------
#define BP_Offset 32
#define CALL_LEN (sizeof(void*) + 1)
static const size_t BP_SourceCave_Min = CALL_LEN;
static const size_t BP_SourceCave_Max = BP_Offset - CALL_LEN;
/// ---------

size_t json_immediate_value(json_t *val, x86_reg_t *regs)
{
	if (!val || json_is_null(val)) {
		return 0;
	}
	if (json_is_integer(val)) {
		return (size_t)json_integer_value(val);
	}
	else if (!json_is_string(val)) {
		log_func_printf("the expression must be either an integer or a string.\n");
		return 0;
	}
	const char *expr = json_string_value(val);
	size_t ret = 0;
	eval_expr(expr, '\0', &ret, regs, NULL);
	return ret;
}

size_t *json_pointer_value(json_t *val, x86_reg_t *regs)
{
	const char *expr = json_string_value(val);
	if (!expr) {
		return NULL;
	}

	// This function returns a pointer to an expression - that means the expression must resolve to something that can be pointed to.
	// We'll accept only 2 kind of expressions:
	// - A dereferencing (for example "[ebp-8]"), where we'll skip the top-level dereferencing. After all, ebp-8 points to [ebp-8].
	// - A register name, without anything else. In that case, we can return a pointer to the register in the x86_reg_t structure.
	size_t *ptr;
	const char *expr_end;

	ptr = reg(regs, expr, &expr_end);
	if (ptr && expr_end[0] == '\0') {
		return ptr;
	}
	else if (expr[0] == '[') {
		expr_end = eval_expr(expr + 1, ']', (size_t*)&ptr, regs, NULL);
		if (expr_end[0] != '\0') {
			log_func_printf("Warning: leftover bytes after dereferencing: '%s'\n", expr_end);
		}
		return ptr;
	}
	log_func_printf("Error: called with something other than a register or a dereferencing.\n");
	return NULL;
}

size_t* json_register_pointer(json_t *val, x86_reg_t *regs)
{
	return reg(regs, json_string_value(val), nullptr);
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

int breakpoint_cave_exec_flag(json_t *bp_info)
{
	return !json_is_false(json_object_get(bp_info, "cave_exec"));
}

size_t breakpoint_process(breakpoint_local_t *bp, x86_reg_t *regs)
{
	assert(bp);

	size_t esp_diff = 0;

	// POPAD ignores the ESP register, so we have to implement our own mechanism
	// to be able to manipulate it.
	size_t esp_prev = regs->esp;

	int cave_exec = bp->func(regs, bp->json_obj);
	if(cave_exec) {
		// Point return address to codecave.
		regs->retaddr = (size_t)bp->cave;
	}
	if(esp_prev != regs->esp) {
		// ESP change requested.
		// Shift down the regs structure by the requested amount
		esp_diff = regs->esp - esp_prev;
		memmove((BYTE*)(regs) + esp_diff, regs, sizeof(x86_reg_t));
	}
	return esp_diff;
}

bool breakpoint_from_json(const char *name, json_t *in, breakpoint_local_t *out) {
	if (!json_is_object(in)) {
		log_printf("breakpoint %s: not an object\n", name);
		return false;
	}

	bool ignore = json_object_get_evaluate_bool(in, "ignore");
	if (ignore) {
		log_printf("breakpoint %s: ignored\n", name);
		return false;
	}

	size_t cavesize = json_object_get_evaluate_int(in, "cavesize");
	if (!cavesize) {
		log_printf("breakpoint %s: no cavesize specified\n", name);
		return false;
	} else if (cavesize < CALL_LEN) {
		log_printf("breakpoint %s: cavesize too small to implement breakpoint\n", name);
		return false;
	}

	out->name = strdup(name);
	out->cavesize = cavesize;
	out->json_obj = json_incref(in);
	out->func = nullptr;
	out->cave = nullptr;

	return true;
}

static inline void __fastcall cave_fix(BYTE *cave, BYTE *bp_addr)
{
	/// Fix relative stuff
	/// ------------------

	// #1: Relative far call / jump at the very beginning
	if(cave[0] == 0xe8 || cave[0] == 0xe9) {
		size_t dist_old = *((size_t*)(cave + 1));
		size_t dist_new = dist_old + bp_addr - cave;

		memcpy(cave + 1, &dist_new, sizeof(dist_new));

		log_printf("fixing rel.addr. 0x%p to 0x%p... ", dist_old, dist_new);
	}
	/// ------------------
}

static bool __fastcall breakpoint_local_init(
	breakpoint_local_t *bp_local
) {

	const char *const key = bp_local->name;
	// Multi-slot support
	const char *const slot = strchr(key, '#');
	const size_t key_len = slot ? (size_t)(slot - key) : strlen(key);

	VLA(char, bp_key, strlen("BP_") + key_len + 1);
	if (strncmp(key, "codecave:", 9) != 0) {
		strcpy(bp_key, "BP_");
		strncat(bp_key, key, key_len);
	} else {
		strcpy(bp_key, key);
	}
	bp_local->func = (BreakpointFunc_t)func_get(bp_key);

	if (!bp_local->func) {
		hackpoints_error_function_not_found(bp_key, 0);
		VLA_FREE(bp_key);
		return false;
	}
	VLA_FREE(bp_key);
	return true;
}


static void __fastcall breakpoint_apply(BYTE* callcave, breakpoint_local_t *bp)
{
	const size_t cave_dist = bp->addr.raw - ((size_t)bp->cave + CALL_LEN);
	const size_t bp_dist = (size_t)(callcave - (bp->addr.raw + CALL_LEN));
	VLA(BYTE, bp_asm, bp->cavesize);

	/// Cave assembly
	// Copy old code to cave
	memcpy(bp->cave, (void*)bp->addr.raw, bp->cavesize);
	cave_fix(bp->cave, (BYTE*)bp->addr.raw);

	// JMP addr
	bp->cave[bp->cavesize] = 0xe9;
	*(size_t*)(bp->cave + bp->cavesize + 1) = cave_dist;

	// CALL bp_entry
	bp_asm[0] = 0xe8;
	*(size_t*)(bp_asm + 1) = bp_dist;
	switch (bp->cavesize - CALL_LEN) {
		case 1:
			*(bp_asm + CALL_LEN) = 0x90;
			break;
		case 2:
			*(uint16_t*)(bp_asm + CALL_LEN) = 0x9066;
			break;
		case 3:
			*(bp_asm + CALL_LEN) = 0x0F;
			*(uint16_t*)(bp_asm + CALL_LEN + 1) = 0x001F;
			break;
		case 4:
			*(uint32_t*)(bp_asm + CALL_LEN) = 0x00401F0F;
			break;
		default:
			memset(bp_asm + CALL_LEN, 0x90, bp->cavesize - CALL_LEN);
		case 0:;
	}

	PatchRegion((void*)bp->addr.raw, NULL, bp_asm, bp->cavesize);
	VLA_FREE(bp_asm);
}

extern "C" void *bp_entry_end;
extern "C" void *bp_entry_localptr;
extern "C" void *bp_call;

int breakpoints_apply(breakpoint_local_t *breakpoints, size_t bp_count, HMODULE hMod)
{
	if(!breakpoints || !bp_count) {
		log_printf("No breakpoints to set up.\n");
		return 0;
	}

	size_t sourcecaves_total_size = 0;
	size_t valid_breakpoint_count = 0;

	struct breakpoint_local_state_t {
		size_t cavesize_full;
		bool skip;
	};

	VLA(breakpoint_local_state_t, breakpoint_local_state, bp_count * sizeof(breakpoint_local_state_t));

	log_print(
		"Setting up breakpoints...\n"
		"-------------------------\n"
	);

	for (size_t i = 0; i < bp_count; ++i) {
		breakpoint_local_t *const cur_bp = &breakpoints[i];

		size_t addr = 0;
		if (cur_bp->addr.type == STR_ADDR) {
			eval_expr(cur_bp->addr.str, '\0', &addr, NULL, (size_t)hMod);
			free(cur_bp->addr.str);
			cur_bp->addr.type = RAW_ADDR;
			cur_bp->addr.raw = addr;
		} else if (cur_bp->addr.type == RAW_ADDR) {
			addr = cur_bp->addr.raw;
		}
		
		log_printf("(%2d/%2d) 0x%p %s... ", i + 1, bp_count, addr, cur_bp->name);

		const size_t cavesize = cur_bp->cavesize;
		if (!addr || !VirtualCheckRegion((const void*)addr, cavesize)) {
			breakpoint_local_state[i].skip = true;
			continue;
		}
		if (breakpoint_local_init(cur_bp)) {
			breakpoint_local_state[i].skip = false;
			breakpoint_local_state[i].cavesize_full = AlignUpToMultipleOf(cavesize, 16);
			sourcecaves_total_size += breakpoint_local_state[i].cavesize_full;
			++valid_breakpoint_count;
			log_printf("OK\n");
		} else {
			breakpoint_local_state[i].skip = true;
		}
	}

	// Call cave construction
	const size_t call_size = (uint8_t*)&bp_entry_end - (uint8_t*)bp_entry;
	const size_t call_size_full = AlignUpToMultipleOf(call_size, 16);
	const size_t localptr_offset = (uint8_t*)&bp_entry_localptr + 1 - (uint8_t*)bp_entry;
	const size_t callptr_offset = (uint8_t*)&bp_call + 1 - (uint8_t*)bp_entry;
	
	uint8_t *const cave_source = (BYTE*)VirtualAlloc(0, sourcecaves_total_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	memset(cave_source, 0xcc, sourcecaves_total_size);
	BYTE *sourcecave_p = cave_source;

	const size_t callcaves_total_size = bp_count * call_size_full;
	uint8_t *const cave_call = (BYTE*)VirtualAlloc(0, callcaves_total_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	log_printf(
		"-------------------------\n",
		"Rendering breakpoints... (source cave at 0x%p, call cave at 0x%p)\n"
		"-------------------------\n",
		cave_source, cave_call
	);

	BYTE *callcave_p = cave_call;
	for (size_t i = 0; i < bp_count; ++i) {
		if (!breakpoint_local_state[i].skip) {
			breakpoint_local_t *const cur_bp = &breakpoints[i];

			memcpy(callcave_p, (uint8_t*)bp_entry, call_size);

			const auto callcave_localptr = (breakpoint_local_t **)(callcave_p + localptr_offset);
			*callcave_localptr = cur_bp;
			const auto bpcall_localptr = (size_t*)(callcave_p + callptr_offset);
			*bpcall_localptr = (size_t)&breakpoint_process - (size_t)bpcall_localptr - sizeof(void*);

			cur_bp->cave = sourcecave_p;
			breakpoint_apply(callcave_p, cur_bp);

			callcave_p += call_size_full;
			sourcecave_p += breakpoint_local_state[i].cavesize_full;
		}
	}

	DWORD idgaf;
	VirtualProtect(cave_source, sourcecaves_total_size, PAGE_EXECUTE_READ, &idgaf);
	VirtualProtect(cave_call, callcaves_total_size, PAGE_EXECUTE_READ, &idgaf);

	return bp_count - valid_breakpoint_count;
}
