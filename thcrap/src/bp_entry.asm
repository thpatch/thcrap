/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoint entry point. Written for i686-w64-mingw32-as.
  */

	.intel_syntax
	.global	_bp_entry, _bp_entry_indexptr, _bp_entry_localptr, _bp_entry_callptr, _bp_entry_end

_bp_entry:
	pusha
	pushf
	cld
	push	%esp
_bp_entry_indexptr:
	push	0x12345678
_bp_entry_localptr:
	push	0x12345678
_bp_entry_callptr:
	call	_bp_entry_callptr
	lea		%esp, [%esp+%eax+0xC]
	popf
	popa
	ret
_bp_entry_end:
