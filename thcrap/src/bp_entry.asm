/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoint entry point. Written for i686-w64-mingw32-as.
  */

	.intel_syntax
	.global	_breakpoint_entry

_breakpoint_entry:
	pusha
	pushf
	push	%esp
	call	_breakpoint_process
	add	%esp, 4
	add	%esp, %eax
	popf
	popa
	ret

// /SAFESEH compliance. Note that the @feat.00 symbol *must not* be global.
	.set @feat.00, 1
