/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoint entry point. Written for i686-w64-mingw32-as.
  */

	.intel_syntax
	.global	_BP_bmpfont_fix_parameters

_BP_bmpfont_fix_parameters:
	push	dword [%esp+4]
	push	dword [%esp+4]
	sub		%esp, 0x0c
	movss	dword [%esp+4], %xmm2
	movss	dword [%esp+0], %xmm1
	movss	dword [%esp-4], %xmm0
	call	_BP_c_bmpfont_fix_parameters
	movss	%xmm0, dword [%esp-4]
	movss	%xmm1, dword [%esp+0]
	movss	%xmm2, dword [%esp+4]
	add		%esp, 0x14
	ret

// /SAFESEH compliance. Note that the @feat.00 symbol *must not* be global.
	.set @feat.00, 1
