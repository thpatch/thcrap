/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Custom exception handler.
  */

#include "thcrap.h"

void log_print_context(PCONTEXT ctx)
{
	if(!ctx) {
		return;
	}
	log_printf(
		"Registers:\n"
		"EAX: 0x%p ECX: 0x%p EDX: 0x%p EBX: 0x%p\n"
		"ESP: 0x%p EBP: 0x%p ESI: 0x%p EDI: 0x%p\n",
		ctx->Eax, ctx->Ecx, ctx->Edx, ctx->Ebx,
		ctx->Esp, ctx->Ebp, ctx->Esi, ctx->Edi
	);
}

void log_print_rva_and_module(HMODULE mod, void *addr)
{
	if(!mod) {
		return;
	}
	size_t crash_fn_len = GetModuleFileNameU(mod, NULL, 0) + 1;
	VLA(char, crash_fn, crash_fn_len);
	if(GetModuleFileNameU(mod, crash_fn, crash_fn_len)) {
		log_printf(
			" (Rx%x) (%s)",
			(uint8_t *)addr - (uint8_t *)mod,
			PathFindFileNameA(crash_fn)
		);
	}
	VLA_FREE(crash_fn);
	log_print("\n");
}

LONG WINAPI exception_filter(LPEXCEPTION_POINTERS lpEI)
{
	LPEXCEPTION_RECORD lpER = lpEI->ExceptionRecord;
	HMODULE crash_mod = GetModuleContaining(lpER->ExceptionAddress);

	log_printf(
		"\n"
		"===\n"
		"Exception %x at 0x%p",
		lpER->ExceptionCode, lpER->ExceptionAddress
	);
	log_print_rva_and_module(crash_mod, lpER->ExceptionAddress);
	log_print_context(lpEI->ContextRecord);
	log_printf(
		"===\n"
		"\n"
	);
	return EXCEPTION_CONTINUE_SEARCH;
}

void exception_init(void)
{
	AddVectoredExceptionHandler(1, exception_filter);
}
