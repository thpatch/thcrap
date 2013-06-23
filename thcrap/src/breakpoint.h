/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoint handling.
  */

#pragma once

// Register structure in PUSHAD order at the beginning of a function
typedef struct {
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

// Returns a pointer to the register [regname] in [regs]
size_t* reg(x86_reg_t *regs, const char *regname);

// Returns a pointer to the register in [regs] specified by [key] in [object]
size_t* json_object_get_register(json_t *object, x86_reg_t *regs, const char *key);

// Main breakpoint hook function. A CALL to this function is written to every
// breakpoint's address.
void breakpoint_process();

// Sets up all breakpoints in [breakpoints].
int breakpoints_apply();

// Removes all breakpoints
int breakpoints_remove();

// Links the breakpoint [bp_name] to the function [func]
int breakpoint_register(const char *bp_name, BreakpointFunc_t func);
