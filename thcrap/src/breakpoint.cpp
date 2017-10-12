/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoint handling.
  */

#include "thcrap.h"

typedef struct {
	// Address where the breakpoint is written
	BYTE *addr;

	// Size of the original code sliced out at [addr].
	// Must be inside BP_CodeCave_Limits.
	size_t cavesize;

	BreakpointFunc_t func;
	json_t *json_obj;

	// First byte of the code cave for this breakpoint.
	BYTE *cave;
} breakpoint_local_t;

// Static global variables
// -----------------------
static BYTE *BP_CodeCave = NULL;
static int BP_Count = 0;
static breakpoint_local_t *BP_Local = NULL;

#define BP_Offset 32
#define CALL_LEN (sizeof(void*) + 1)
static const size_t BP_CodeCave_Limits[2] = {CALL_LEN, (BP_Offset - CALL_LEN)};
// -----------------------

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
	memcpy(&cmp, regname, 4);
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

static size_t eval_expr(const char **expr_ptr, x86_reg_t *regs, char end)
{
	const char *expr = *expr_ptr;
	size_t value = NULL;
	char op = '+';

	log_printf("entering eval_expr: '%s'\n", expr);
	while (*expr && *expr != end) {
		//log_printf("while: '%s'\n", expr);
		if (strchr("+-*/%", *expr)) {
			//log_printf("op: '%s'\n", expr);
			op = *expr;
			expr++;
			continue;
		}

		size_t cur_value;
		if (*expr == '[') {
			//log_printf("[: '%s'\n", expr);
			expr++;
			cur_value = eval_expr(&expr, regs, ']');
			if (cur_value) {
				cur_value = *(size_t*)cur_value;
			}
		}
		else if ((cur_value = (size_t)reg(regs, expr, &expr)) != 0) {
			cur_value = *(size_t*)cur_value;
			log_printf("register is %x\n", cur_value);
		}
		else {
			str_address_ret_t addr_ret;
			cur_value = str_address_value(expr, nullptr, &addr_ret);
			if(addr_ret.error) {
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
		}
		log_printf("sum is %x\n", value);
	}

	if (end == ']' && *expr != end) {
		log_printf("Error while evaluating expression around '%s': '[' without matching ']'.\n", expr);
		return 0;
	}

	expr++;
	*expr_ptr = expr;
	log_printf("exiting with value %x\n", value);
	return value;
}

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
	return eval_expr(&expr, regs, '\0');
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
		ptr = (size_t*)eval_expr(&expr, regs, ']');
		if (*expr != '\0') {
			log_func_printf("Warning: leftover bytes after dereferencing: '%s'\n", expr);
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

size_t breakpoint_process(x86_reg_t *regs)
{
	breakpoint_local_t *bp = NULL;
	size_t esp_diff = 0;
	int i;
	int cave_exec = 1;

	// POPAD ignores the ESP register, so we have to implement our own mechanism
	// to be able to manipulate it.
	size_t esp_prev = regs->esp;

	for(i = 0; (i < BP_Count) && !bp; i++) {
		if(BP_Local[i].addr == UlongToPtr(regs->retaddr - CALL_LEN)) {
			bp = &BP_Local[i];
		}
	}
	if(bp) {
		assert(bp->func);
		cave_exec = bp->func(regs, bp->json_obj);
	}
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

int breakpoint_local_init(breakpoint_local_t *bp_local, json_t *bp_json, size_t addr, const char *key, size_t index)
{
	size_t cavesize = json_object_get_hex(bp_json, "cavesize");

	if(!bp_local || !bp_json || !addr || !key) {
		return -1;
	}
	ZeroMemory(bp_local, sizeof(*bp_local));

	if(!cavesize) {
		log_printf("ERROR: no cavesize specified!\n");
		return 2;
	}
	if(cavesize < BP_CodeCave_Limits[0] || cavesize > BP_CodeCave_Limits[1]) {
		log_printf("ERROR: cavesize exceeds limits! (given: %d, min: %d, max: %d)\n",
			cavesize, BP_CodeCave_Limits[0], BP_CodeCave_Limits[1]);
		return 3;
	}

	STRLEN_DEC(key);
	VLA(char, bp_key, key_len + strlen("BP_") + 1);
	int ret = 0;

	// Multi-slot support
	const char *slot = strchr(key, '#');
	if(slot) {
		key_len = slot - key;
	}

	strcpy(bp_key, "BP_");
	strncat(bp_key, key, key_len);
	bp_local->func = (BreakpointFunc_t)func_get(bp_key);

	bp_local->addr = (BYTE*)addr;
	if(!bp_local->func) {
		ret = hackpoints_error_function_not_found(bp_key, 4);
	}
	bp_local->cavesize = cavesize;
	bp_local->cave = BP_CodeCave + (index * BP_Offset);
	bp_local->json_obj = bp_json;
	VLA_FREE(bp_key);
	return ret;
}

int breakpoint_apply(breakpoint_local_t *bp)
{
	if(bp) {
		size_t cave_dist = bp->addr - (bp->cave + CALL_LEN);
		size_t bp_dist = (BYTE*)breakpoint_entry - (bp->addr + CALL_LEN);
		BYTE bp_asm[BP_Offset];

		/// Cave assembly
		// Copy old code to cave
		memcpy(bp->cave, UlongToPtr(bp->addr), bp->cavesize);
		cave_fix(bp->cave, bp->addr);

		// JMP addr
		bp->cave[bp->cavesize] = 0xe9;
		memcpy(bp->cave + bp->cavesize + 1, &cave_dist, sizeof(cave_dist));

		/// Breakpoint assembly
		memset(bp_asm, 0x90, bp->cavesize);
		// CALL breakpoint_entry
		bp_asm[0] = 0xe8;
		memcpy(bp_asm + 1, &bp_dist, sizeof(void*));

		PatchRegion(bp->addr, NULL, bp_asm, bp->cavesize);
		log_printf("OK\n");
		return 0;
	}
	return -1;
}

int breakpoints_apply(json_t *breakpoints)
{
	const char *key;
	json_t *json_bp;
	size_t breakpoint_count = hackpoints_count(breakpoints);
	int i = -1;

	if(!breakpoints) {
		return -1;
	}
	if(!breakpoint_count) {
		log_printf("No breakpoints to set up.\n");
		return 0;
	}
	// Don't set up twice
	if(BP_CodeCave || BP_Local) {
		log_printf("Breakpoints already set up.\n");
		return 0;
	}
	BP_Count = breakpoint_count;
	BP_Local = (breakpoint_local_t *)calloc(BP_Count, sizeof(breakpoint_local_t));
	BP_CodeCave = (BYTE*)VirtualAlloc(0, breakpoint_count * BP_Offset, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	memset(BP_CodeCave, 0xcc, breakpoint_count * BP_Offset);

	log_printf("Setting up breakpoints... (code cave at 0x%p)\n", BP_CodeCave);
	log_printf("-------------------------\n");

	json_object_foreach(breakpoints, key, json_bp) {
		size_t j;
		json_t *addr_val;
		auto ignore = json_object_get(json_bp, "ignore");
		if(json_is_true(ignore)) {
			continue;
		}
		json_flex_array_foreach(json_object_get(json_bp, "addr"), j, addr_val) {
			size_t addr = json_hex_value(addr_val);
			if(addr) {
				breakpoint_local_t *bp = &BP_Local[++i];
				log_printf("(%2d/%2d) 0x%p %s... ", i + 1, BP_Count, addr, key);
				if(!breakpoint_local_init(bp, json_bp, addr, key, i)) {
					breakpoint_apply(bp);
				}
			}
		}
	}
	log_printf("-------------------------\n");
	return 0;
}

int breakpoints_remove(void)
{

	if(!BP_Count || !BP_Local) {
		log_printf("No breakpoints to remove.\n");
		return 0;
	}

	log_printf(
		"Removing breakpoints...\n"
		"-----------------------\n"
	);
	// TODO: Implement!

	VirtualFree(BP_CodeCave, 0, MEM_RELEASE);
	SAFE_FREE(BP_Local);
	BP_Count = 0;
	return 0;
}
