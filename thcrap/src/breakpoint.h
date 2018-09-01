/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoint handling.
  */

#pragma once

// Register structure in PUSHAD+PUSHFD order at the beginning of a function
typedef struct {
	size_t flags;
	size_t edi;
	size_t esi;
	size_t ebp;
	size_t esp;
	size_t ebx;
	size_t edx;
	size_t ecx;
	size_t eax;
	size_t retaddr;
} x86_reg_t;

/**
  * Breakpoint function type.
  * As these are looked up without any manual registration, the name of a
  * breakpoint function *must* be prefixed with "BP_".
  *
  * Parameters
  * ----------
  *	x86_reg_t *regs
  *		x86 general purpose registers at the time of the breakpoint.
  *		Can be read and written.
  *
  *	json_t *bp_info
  *		The breakpoint's JSON object in the run configuration.
  *
  * Return value
  * ------------
  *	1 - to execute the breakpoint codecave.
  *	0 - to not execute the breakpoint codecave.
  *	    In this case, the retaddr element of [regs] can be manipulated to
  *	    specify a different address to resume code execution after the breakpoint.
  */
typedef int (*BreakpointFunc_t)(x86_reg_t *regs, json_t *bp_info);

typedef struct {
	// Address where the breakpoint is written
	uint8_t *addr;

	// Size of the original code sliced out at [addr].
	// Must be inside BP_SourceCave_Limits.
	size_t cavesize;

	BreakpointFunc_t func;
	json_t *json_obj;

	// First byte of the code cave for this breakpoint.
	uint8_t *cave;
} breakpoint_local_t;

typedef struct {
	breakpoint_local_t *bp_local;
	size_t bp_count;

	// Contains the original code bytes we overwrote
	uint8_t *cave_source;

	// Contains the calls with the correct breakpoint_local_t* for each
	// breakpoint.
	uint8_t *cave_call;
} breakpoint_set_t;

// Returns a pointer to the register [regname] in [regs]. [endptr] behaves
// like the endptr parameter of strtol(), and can be a nullptr if not needed.
size_t* reg(x86_reg_t *regs, const char *regname, const char **endptr);

/// Register and memory values from JSON
/// ====================================
// Calls reg() on the JSON string [val].
size_t* json_register_pointer(json_t *val, x86_reg_t *regs);

// Evaluate the JSON string [val] as an expression.
size_t json_immediate_value(json_t *val, x86_reg_t *regs);

// Evaluate the JSON string [val] as an expression, and returns a pointer to the result.
size_t* json_pointer_value(json_t *val, x86_reg_t *regs);

// Calls json_register_pointer() on the value of [key] in [object].
size_t* json_object_get_register(json_t *object, x86_reg_t *regs, const char *key);

// Calls json_pointer_value() on the value of [key] in [object].
size_t* json_object_get_pointer(json_t *object, x86_reg_t *regs, const char *key);

// Calls json_immediate_value() on the value of [key] in [object].
size_t json_object_get_immediate(json_t *object, x86_reg_t *regs, const char *key);
/// =====================================

// Returns 0 if "cave_exec" in [bp_info] is set to false, 1 otherwise.
// Should be used as the return value for a breakpoint function after it made
// changes to a register which could require original code to be skipped
// (since that code might overwrite the modified data otherwise).
int breakpoint_cave_exec_flag(json_t *bp_info);

// Sets up all breakpoints in [breakpoints], and returns the number of
// breakpoints that could not be applied. [hMod] is used as the base
// for relative addresses.
int breakpoints_apply(breakpoint_set_t *set, json_t *breakpoints, HMODULE hMod);

// Removes all breakpoints in the given set.
// TODO: Implement!
// int breakpoints_remove();
