/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Expression parsing
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

// Returns a pointer to the register [regname] in [regs]. [endptr] behaves
// like the endptr parameter of strtol(), and can be a nullptr if not needed.
size_t* reg(x86_reg_t *regs, const char *regname, const char **endptr);

const char* parse_brackets(const char* str, char opening);

//size_t eval_expr(const char** expr_ptr, x86_reg_t *const regs, char end, size_t rel_source);

const char* eval_expr(const char* expr, size_t* out, char end, x86_reg_t* regs, size_t rel_source);
