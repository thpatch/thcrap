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
	eval_expr(expr, &ret, '\0', regs, NULL);
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
	const char *reg_end;

	ptr = reg(regs, expr, &reg_end);
	if (ptr && reg_end[0] == '\0') {
		return ptr;
	}
	else if (expr[0] == '[') {
		expr++;
		//ptr = (size_t*)eval_expr(&expr, regs, ']', NULL);
		eval_expr(expr, (size_t*)&ptr, ']', regs, NULL);
		/*if (*expr != '\0') {
			log_func_printf("Warning: leftover bytes after dereferencing: '%s'\n", expr);
		}*/
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

void cave_fix(BYTE *cave, BYTE *bp_addr)
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

int breakpoint_local_init(
	breakpoint_local_t *bp_local,
	size_t addr,
	uint8_t *cave_source
)
{
	if(!bp_local || !addr) {
		return -1;
	}

	if(bp_local->cavesize < BP_SourceCave_Min || bp_local->cavesize > BP_SourceCave_Max) {
		log_printf("ERROR: cavesize exceeds limits! (given: %d, min: %d, max: %d)\n",
			bp_local->cavesize, BP_SourceCave_Min, BP_SourceCave_Max);
		return 3;
	}

	const char *key = bp_local->name;
	STRLEN_DEC(key);
	VLA(char, bp_key, key_len + strlen("BP_") + 1);
	int ret = 0;

	// Multi-slot support
	const char *slot = strchr(key, '#');
	if(slot) {
		key_len = slot - key;
	}

	if (strncmp(key, "codecave:", 9) != 0) {
		strcpy(bp_key, "BP_");
		strncat(bp_key, key, key_len);
	} else {
		strcpy(bp_key, key);
	}
	bp_local->func = (BreakpointFunc_t)func_get(bp_key);

	bp_local->addr = (BYTE*)addr;
	if(!bp_local->func) {
		ret = hackpoints_error_function_not_found(bp_key, 4);
	}
	bp_local->cave = cave_source;
	VLA_FREE(bp_key);
	return ret;
}

int breakpoint_apply(BYTE* callcave, breakpoint_local_t *bp)
{
	if(bp) {
		size_t cave_dist = bp->addr - (bp->cave + CALL_LEN);
		size_t bp_dist = (BYTE*)callcave - (bp->addr + CALL_LEN);
		BYTE bp_asm[BP_Offset];

		/// Cave assembly
		// Copy old code to cave
		memcpy(bp->cave, (void*)bp->addr, bp->cavesize);
		cave_fix(bp->cave, bp->addr);

		// JMP addr
		bp->cave[bp->cavesize] = 0xe9;
		memcpy(bp->cave + bp->cavesize + 1, &cave_dist, sizeof(cave_dist));

		/// Breakpoint assembly
		memset(bp_asm, 0x90, bp->cavesize);
		// CALL bp_entry
		bp_asm[0] = 0xe8;
		memcpy(bp_asm + 1, &bp_dist, sizeof(void*));

		PatchRegion(bp->addr, NULL, bp_asm, bp->cavesize);
		log_printf("OK\n");
		return 0;
	}
	return -1;
}

extern "C" void *bp_entry_end;
extern "C" void *bp_entry_localptr;

int breakpoints_apply(breakpoint_local_t *breakpoints, size_t bp_count, HMODULE hMod)
{
	int failed = bp_count;

	if(!breakpoints || !bp_count) {
		log_printf("No breakpoints to set up.\n");
		return 0;
	}
	uint8_t *cave_source = (BYTE*)VirtualAlloc(0, bp_count * BP_Offset, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	memset(cave_source, 0xcc, bp_count * BP_Offset);

	// Call cave construction
	size_t call_size = (uint8_t*)&bp_entry_end - (uint8_t*)bp_entry;
	size_t localptr_offset = (uint8_t*)&bp_entry_localptr + 1 - (uint8_t*)bp_entry;
	uint8_t *cave_call = (BYTE*)VirtualAlloc(0, bp_count * call_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

	log_printf(
		"Setting up breakpoints... (source cave at 0x%p, call cave at 0x%p)\n"
		"-------------------------\n",
		cave_source, cave_call
	);

	BYTE *callcave_p = cave_call;
	for (size_t i = 0; i < bp_count; i++) {
		breakpoint_local_t *bp = &breakpoints[i];
		auto addr = str_address_value(bp->addr_str, hMod, nullptr);

		log_printf("(%2d/%2d) 0x%p %s... ", i + 1, bp_count, addr, bp->name);

		if (!addr || !VirtualCheckRegion((const void*)addr, CALL_LEN)) {
			continue;
		}

		if (!breakpoint_local_init(
			bp, addr, cave_source + (i * BP_Offset)
		)) {
			memcpy(callcave_p, (uint8_t*)bp_entry, call_size);
			auto callcave_localptr = (breakpoint_local_t **)(callcave_p + localptr_offset);
			*callcave_localptr = bp;

			breakpoint_apply(callcave_p, bp);

			callcave_p += call_size;
			failed--;
		}
	}
	log_printf("-------------------------\n");
	return failed;
}
