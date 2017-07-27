/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoint entry point. Written for i686-w64-mingw32-as.
  */

	.intel_syntax
	.global	_bp_entry, _bp_entry_localptr, _bp_entry_end

_bp_entry:
	pusha
	pushf
	push	%esp
_bp_entry_localptr:
	push	0x12345678
	/* Since we need to be position-independent... */
	mov %eax, offset _breakpoint_process
	call	eax
	add	%esp, 8
	add	%esp, %eax
	popf
	popa
	ret
_bp_entry_end:

// /SAFESEH compliance. Note that the @feat.00 symbol *must not* be global.
	.set @feat.00, 1
