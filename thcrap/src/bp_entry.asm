/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoint entry point. Written for i686-w64-mingw32-as.
  */

	.intel_syntax
	.global	_bp_entry, _bp_entry_caveptr, _bp_entry_localptr, _bp_entry_callptr, _bp_entry_end

_bp_entry:
	pusha
	pushf
	cld
	push	esp
	/* TODO: Make these push a dword value of 0. Even "pushl $0" with AT&T syntax switches to the byte encoding instead of dword. */
_bp_entry_caveptr:
	push	0xDEADBEEF
_bp_entry_localptr:
	push	0xDEADBEEF
_bp_entry_callptr:
	call	_breakpoint_process
	lea		esp, [esp+eax+0xC]
	popf
	popa
	ret
	.balign 16, 0xCC
_bp_entry_end:
