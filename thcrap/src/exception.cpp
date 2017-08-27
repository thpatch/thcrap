/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Custom exception handler.
  */

#include "thcrap.h"

void log_context_dump(PCONTEXT ctx)
{
	if(!ctx) {
		return;
	}
	log_printf(
		"Registers:\n"
		"EAX: 0x%08x ECX: 0x%08x EDX: 0x%08x EBX: 0x%08x\n"
		"ESP: 0x%08x EBP: 0x%08x ESI: 0x%08x EDI: 0x%08x\n",
		ctx->Eax, ctx->Ecx, ctx->Edx, ctx->Ebx,
		ctx->Esp, ctx->Ebp, ctx->Esi, ctx->Edi
	);
}

LONG WINAPI exception_filter(LPEXCEPTION_POINTERS lpEI)
{
	LPEXCEPTION_RECORD lpER = lpEI->ExceptionRecord;
	HMODULE crash_mod = GetModuleContaining(lpER->ExceptionAddress);

	log_printf(
		"\n"
		"===\n"
		"Exception %x at 0x%08x",
		lpER->ExceptionCode, lpER->ExceptionAddress
	);
	if(crash_mod) {
		size_t crash_fn_len = GetModuleFileNameU(crash_mod, NULL, 0) + 1;
		VLA(char, crash_fn, crash_fn_len);
		if(GetModuleFileNameU(crash_mod, crash_fn, crash_fn_len)) {
			log_printf(
				" (Rx%x) (%s)",
				(DWORD)lpER->ExceptionAddress - (DWORD)crash_mod,
				PathFindFileNameA(crash_fn)
			);
		}
		VLA_FREE(crash_fn);
	}
	log_printf("\n");
	log_context_dump(lpEI->ContextRecord);
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
