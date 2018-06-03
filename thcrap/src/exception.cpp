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
		"\nRegisters:\n"
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
			PathFindFileNameU(crash_fn)
		);
	}
	VLA_FREE(crash_fn);
	log_print("\n");
}

LONG WINAPI exception_filter(LPEXCEPTION_POINTERS lpEI)
{
	LPEXCEPTION_RECORD lpER = lpEI->ExceptionRecord;

	// These should be all that matter. Unfortunately, we tend
	// to get way more than we want, particularly with wininet.
	switch(lpER->ExceptionCode) {
	case STATUS_ACCESS_VIOLATION:
	case STATUS_ILLEGAL_INSTRUCTION:
	case STATUS_INTEGER_DIVIDE_BY_ZERO:
	case 0xE06D7363: /* MSVCRT exceptions, doesn't seem to have a name */
		break;
	default:
		return EXCEPTION_CONTINUE_SEARCH;
	}

	HMODULE crash_mod = GetModuleContaining(lpER->ExceptionAddress);
	log_printf(
		"\n"
		"===\n"
		"Exception %x at 0x%p",
		lpER->ExceptionCode, lpER->ExceptionAddress
	);
	log_print_rva_and_module(crash_mod, lpER->ExceptionAddress);
	log_print_context(lpEI->ContextRecord);

	// "Windows Server 2003 and Windows XP: The sum of the FramesToSkip
	// and FramesToCapture parameters must be less than 63."
	// https://msdn.microsoft.com/en-us/library/windows/desktop/bb204633(v=vs.85).aspx
	const DWORD frames_to_skip = 1;
	void *trace[62 - frames_to_skip];

	USHORT captured = CaptureStackBackTrace(
		frames_to_skip, elementsof(trace), trace, nullptr
	);
	if(captured) {
		log_printf("\nStack trace:\n");

		decltype(captured) skip = 0;
		if(crash_mod) {
			while(
				GetModuleContaining(trace[skip]) != crash_mod
				&& skip < captured
			) {
				skip++;
			}
		}
		if(skip == captured) {
			skip = 0;
		}
		for(decltype(captured) i = skip; i < captured; i++) {
			HMODULE mod = GetModuleContaining(trace[i]);
			log_printf("[%02u] 0x%p", captured - i, trace[i]);
			log_print_rva_and_module(mod, trace[i]);
		}
	}
	log_printf(
		"===\n"
		"\n"
	);
	return EXCEPTION_CONTINUE_SEARCH;
}

void exception_init(void)
{
	AddVectoredExceptionHandler(0, exception_filter);
}
