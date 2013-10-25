/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Custom exception handler.
  */

#include "thcrap.h"

static LPTOP_LEVEL_EXCEPTION_FILTER lpOrigFilter = NULL;

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
	HMODULE crash_mod;
	LPEXCEPTION_RECORD lpER = lpEI->ExceptionRecord;

	log_printf(
		"\n"
		"===\n"
		"Exception %x at 0x%08x",
		lpER->ExceptionCode, lpER->ExceptionAddress
	);
	if(GetModuleHandleEx(
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
		(LPTSTR)lpER->ExceptionAddress,
		&crash_mod
	)) {
		size_t crash_fn_len = GetModuleFileNameU(crash_mod, NULL, 0);
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
	if(lpOrigFilter) {
		lpOrigFilter(lpEI);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

LPTOP_LEVEL_EXCEPTION_FILTER WINAPI exception_SetUnhandledExceptionFilter(
	__in_opt LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter
)
{
	// Don't return our own filter, since this might cause an infinite loop if
	// the game process caches it.
	LPTOP_LEVEL_EXCEPTION_FILTER ret = SetUnhandledExceptionFilter(exception_filter);
	lpOrigFilter = lpTopLevelExceptionFilter;
	return ret == exception_filter ? NULL : ret;
}

void exception_init(void)
{
	SetUnhandledExceptionFilter(exception_filter);
}

void exception_detour(HMODULE hMod)
{
	iat_detour_funcs_var(hMod, "kernel32.dll", 1,
		"SetUnhandledExceptionFilter", exception_SetUnhandledExceptionFilter
	);
}

void exception_exit(void)
{
	SetUnhandledExceptionFilter(lpOrigFilter);
}
