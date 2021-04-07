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
#ifdef __x86_64__
	log_printf(
		"\nRegisters:\n"
		"RAX: 0x%p RCX: 0x%p RDX: 0x%p RBX: 0x%p\n"
		"RSP: 0x%p RBP: 0x%p RSI: 0x%p RDI: 0x%p\n",
		ctx->Rax, ctx->Rcx, ctx->Rdx, ctx->Rbx,
		ctx->Rsp, ctx->Rbp, ctx->Rsi, ctx->Rdi
	);
#else
	log_printf(
		"\nRegisters:\n"
		"EAX: 0x%p ECX: 0x%p EDX: 0x%p EBX: 0x%p\n"
		"ESP: 0x%p EBP: 0x%p ESI: 0x%p EDI: 0x%p\n",
		ctx->Eax, ctx->Ecx, ctx->Edx, ctx->Ebx,
		ctx->Esp, ctx->Ebp, ctx->Esi, ctx->Edi
	);
#endif
}

void log_print_rva_and_module(HMODULE mod, void *addr)
{
	if(!mod) {
		log_print("\n");
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

// These defines are from internal CRT header ehdata.h
// MSVC++ EH exception number
#define EH_EXCEPTION_NUMBER 0xE06D7363 // ('msc' | 0xE0000000)
#define EH_MAGIC_NUMBER1 0x19930520

static const char *get_cxx_eh_typename(LPEXCEPTION_RECORD lpER)
{
	// https://bytepointer.com/resources/old_new_thing/20100730_217_decoding_the_parameters_of_a_thrown_c_exception_0xe06d7363.htm
	// https://www.geoffchappell.com/studies/msvc/language/predefined/
	// http://www.openrce.org/articles/full_view/21

	ULONG_PTR base;
#ifdef __x86_64__
	if (lpER->NumberParameters < 4)
		return nullptr;
	base = lpER->ExceptionInformation[3];
#else
	if (lpER->NumberParameters < 3)
		return nullptr;
	base = 0;
#endif

	if (lpER->ExceptionCode != EH_EXCEPTION_NUMBER)
		return nullptr;
	if (lpER->ExceptionInformation[0] != EH_MAGIC_NUMBER1)
		return nullptr;

	DWORD *throwInfo = (DWORD*)(base + lpER->ExceptionInformation[2]);
	DWORD *catchableTypeArray = (DWORD*)(base + throwInfo[3]);
	if (catchableTypeArray[0] < 1) // array size
		return nullptr;
	DWORD *catchableType = (DWORD*)(base + catchableTypeArray[1]);
	void **typeDescriptor = (void**)(base + catchableType[1]);
	return (const char*)(&typeDescriptor[2]);
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
	case EH_EXCEPTION_NUMBER:
		break;
	default:
		return EXCEPTION_CONTINUE_SEARCH;
	}

	const char *name = get_cxx_eh_typename(lpER);

	// Only show Cn::XH exceptions once because on some PCs CoreUIComponents.dll
	// and CoreMessaging.dll throw a lot of those for whatever reason. See GH-129.
	// I have no clue what Cn::XH is, but it seems to be a wrapper around
	// System::Exception (again, no clue if that's related to CLR)
	if (name && !strcmp(name, ".?AUXH@Cn@@")) { // struct Cn::XH
		static LONG count = 0;
		if (1 == InterlockedCompareExchange(&count, 1, 0))
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
	if (name)
		log_printf("C++ EH type: %s\n", name);
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
