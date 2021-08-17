/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Custom exception handler.
  */

#include "thcrap.h"
#include "intrin.h"

enum ThExceptionDetailLevel : uint8_t {
	Basic		= 0b00000001, // General registers, error code, stack trace
	FPU			= 0b00000010, // x87 stack, x87 control/status words
	SSE			= 0b00000100, // MXCSR, XMM registers
	Segments	= 0b00001000, // Segment registers
	Debug		= 0b00010000, // Debug registers

	Extra		= 0b10000000, // 

	Full		= 0b01111111, // All reasonable information
	Extreme		= 0b11111111  // All possible information
};

static uint8_t exception_detail_level = ThExceptionDetailLevel::Full;

void set_exception_detail(uint8_t detail_level) {
	exception_detail_level = detail_level;
}

#pragma optimize("y", off)
static void log_print_context(CONTEXT* ctx)
{
	if (ctx) {
		if (exception_detail_level & ThExceptionDetailLevel::Basic) {
#ifdef TH_X64
			log_printf(
				"\n"
				"Registers:\n"
				"RIP: 0x%p EFLAGS: 0x%08X\n"
				"RAX: 0x%p RCX: 0x%p RDX: 0x%p RBX: 0x%p\n"
				"RSP: 0x%p RBP: 0x%p RSI: 0x%p RDI: 0x%p\n"
				"R8:  0x%p R9:  0x%p R10: 0x%p R11: 0x%p\n"
				"R12: 0x%p R13: 0x%p R14: 0x%p R15: 0x%p\n"
				, ctx->Rip, ctx->EFlags // No idea why this isn't RFlags
				, ctx->Rax, ctx->Rcx, ctx->Rdx, ctx->Rbx
				, ctx->Rsp, ctx->Rbp, ctx->Rsi, ctx->Rdi
				, ctx->R8,  ctx->R9,  ctx->R10, ctx->R11
				, ctx->R12, ctx->R13, ctx->R14, ctx->R15
			);
#else
			log_printf(
				"\n"
				"Registers:\n"
				"EIP: 0x%p EFLAGS: 0x%p\n"
				"EAX: 0x%p ECX: 0x%p EDX: 0x%p EBX: 0x%p\n"
				"ESP: 0x%p EBP: 0x%p ESI: 0x%p EDI: 0x%p\n"
				, ctx->Eip, ctx->EFlags
				, ctx->Eax, ctx->Ecx, ctx->Edx, ctx->Ebx
				, ctx->Esp, ctx->Ebp, ctx->Esi, ctx->Edi
			);
#endif
		}
		if (exception_detail_level & ThExceptionDetailLevel::Segments) {
			static constexpr const char* segment_types[] = {
				"Read Only",
				"Read Only (Accessed)",
				"Read/Write",
				"Read/Write (Accessed)",
				"Read Only (Expand Down)",
				"Read Only (Expand Down) (Accessed)",
				"Read/Write (Expand Down)",
				"Read/Write (Expand Down) (Accessed)",
				"Execute Only",
				"Execute Only (Accessed)",
				"Execute/Read",
				"Execute Read (Accessed)",
				"Execute Only (Conforming)",
				"Execute Only (Conforming) (Accessed)",
				"Execute/Read (Conforming)",
				"Execute/Read (Conforming) (Accessed)",
#ifdef TH_X64
				"Reserved", "Reserved",
				"LDT",
				"Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
				"64-bit TSS (Available)",
				"Reserved",
				"64-bit TSS (Busy)",
				"64-bit Call Gate",
				"Reserved",
				"64-bit Interrupt Gate",
				"64-bit Trap Gate"
#else
				"Reserved",
				"16-bit TSS (Available)",
				"LDT",
				"16-bit TSS (Busy)",
				"16-bit Call Gate",
				"16-bit/32-bit Task Gate",
				"16-bit Interrupt Gate",
				"16-Bit Trap Gate",
				"Reserved",
				"32-bit TSS (Available)",
				"Reserved",
				"32-bit TSS (Busy)",
				"32-bit Call Gate",
				"Reserved",
				"32-bit Interrupt Gate",
				"32-bit Trap Gate"
#endif
			};
			if (exception_detail_level & ThExceptionDetailLevel::Extra) {
#pragma pack(push)
#pragma pack(1)
				union SegmentDescriptorData {
					uint32_t data_in;
					struct {
						uint32_t dummy_base_A : 8;
						uint32_t segment_type : 5;
						uint32_t protection_level : 2;
						uint32_t present : 1;
						uint32_t dummy_limit : 4;
						uint32_t unused_trash_bit : 1;
						uint32_t long_flag : 1;
						uint32_t data_size : 1;
						uint32_t granularity : 1;
						uint32_t dummy_base_B : 8;
					};
				};
#pragma pack(pop)
				SegmentDescriptorData CSData;
				SegmentDescriptorData SSData;
				SegmentDescriptorData DSData;
				SegmentDescriptorData ESData;
				SegmentDescriptorData FSData;
				SegmentDescriptorData GSData;
#pragma warning(push)
#pragma warning(disable : 4537)
				__asm {
					LAR EAX, [ctx].SegCs
					MOV [CSData], EAX
					LAR EAX, [ctx].SegSs
					MOV [SSData], EAX
					LAR EAX, [ctx].SegDs
					MOV [DSData], EAX
					LAR EAX, [ctx].SegEs
					MOV [ESData], EAX
					LAR EAX, [ctx].SegFs
					MOV [FSData], EAX
					LAR EAX, [ctx].SegGs
					MOV [GSData], EAX
				}
#pragma warning(pop)
				log_printf(
					"\n"
					"CS: 0x%04hX (Limit: %8X (%s), Data: %s, Priv: %hhu, Type: %s%s)\n"
					"SS: 0x%04hX (Limit: %8X (%s), Data: %s, Priv: %hhu, Type: %s%s)\n"
					"DS: 0x%04hX (Limit: %8X (%s), Data: %s, Priv: %hhu, Type: %s%s)\n"
					"ES: 0x%04hX (Limit: %8X (%s), Data: %s, Priv: %hhu, Type: %s%s)\n"
					"FS: 0x%04hX (Limit: %8X (%s), Data: %s, Priv: %hhu, Type: %s%s)\n"
					"GS: 0x%04hX (Limit: %8X (%s), Data: %s, Priv: %hhu, Type: %s%s)\n"
					, ctx->SegCs, __segmentlimit(ctx->SegCs), CSData.granularity ? "Pages" : "Bytes", CSData.data_size ? "32-bit" : CSData.long_flag ? "64-bit" : "16-bit", CSData.protection_level, segment_types[CSData.segment_type], CSData.present ? "" : " (Not Present)"
					, ctx->SegSs, __segmentlimit(ctx->SegSs), SSData.granularity ? "Pages" : "Bytes", SSData.data_size ? "32-bit" : SSData.long_flag ? "64-bit" : "16-bit", SSData.protection_level, segment_types[SSData.segment_type], SSData.present ? "" : " (Not Present)"
					, ctx->SegDs, __segmentlimit(ctx->SegDs), DSData.granularity ? "Pages" : "Bytes", DSData.data_size ? "32-bit" : DSData.long_flag ? "64-bit" : "16-bit", DSData.protection_level, segment_types[DSData.segment_type], DSData.present ? "" : " (Not Present)"
					, ctx->SegEs, __segmentlimit(ctx->SegEs), ESData.granularity ? "Pages" : "Bytes", ESData.data_size ? "32-bit" : ESData.long_flag ? "64-bit" : "16-bit", ESData.protection_level, segment_types[ESData.segment_type], ESData.present ? "" : " (Not Present)"
					, ctx->SegFs, __segmentlimit(ctx->SegFs), FSData.granularity ? "Pages" : "Bytes", FSData.data_size ? "32-bit" : FSData.long_flag ? "64-bit" : "16-bit", FSData.protection_level, segment_types[FSData.segment_type], FSData.present ? "" : " (Not Present)"
					, ctx->SegGs, __segmentlimit(ctx->SegGs), GSData.granularity ? "Pages" : "Bytes", GSData.data_size ? "32-bit" : GSData.long_flag ? "64-bit" : "16-bit", GSData.protection_level, segment_types[GSData.segment_type], GSData.present ? "" : " (Not Present)"
				);
			}
			else {
				log_printf(
					"\n"
					"CS: 0x%04hX (%X)\n"
					"SS: 0x%04hX (%X)\n"
					"DS: 0x%04hX (%X)\n"
					"ES: 0x%04hX (%X)\n"
					"FS: 0x%04hX (%X)\n"
					"GS: 0x%04hX (%X)\n"
					, ctx->SegCs, __segmentlimit(ctx->SegCs)
					, ctx->SegSs, __segmentlimit(ctx->SegSs)
					, ctx->SegDs, __segmentlimit(ctx->SegDs)
					, ctx->SegEs, __segmentlimit(ctx->SegEs)
					, ctx->SegFs, __segmentlimit(ctx->SegFs)
					, ctx->SegGs, __segmentlimit(ctx->SegGs)
				);
			}
		}
		static constexpr const char* rounding_strings[] = {
			"Round to Nearest",
			"Round Down",
			"Round Up",
			"Round to Zero"
		};
		if (exception_detail_level & ThExceptionDetailLevel::FPU) {

			static constexpr const char* fpu_precision_strings[] = {
				"Single(24)",
				"Invalid",
				"Double(53)",
				"Extended(64)"
			};

			static constexpr const char* fpu_tag_strings[] = {
				"Valid",
				"Zero ",
				"Other",
				"Empty"
			};

#define TagWordString(index) \
		fpu_tag_strings[(ctx->FloatSave.TagWord >> ((index) * 2)) & 0b11]

#define FPUReg(index) \
		*(uint16_t*)&ctx->FloatSave.RegisterArea[(10 * (index)) + sizeof(uint64_t)], *(uint64_t*)&ctx->FloatSave.RegisterArea[10 * (index)]


			if (exception_detail_level & ThExceptionDetailLevel::Extra) {
				XSAVE_FORMAT* fxsave_ctx = (XSAVE_FORMAT*)ctx->ExtendedRegisters;
				if (CPU_FCS_FDS_Deprecated()) {
					log_printf(
						"\n"
						"FOP: %03hX\n"
						"FCS: ---- FIP: 0x%08X\n"
						"FDS: ---- FDP: 0x%08X"
						, fxsave_ctx->ErrorOpcode & 0x7FF
						, fxsave_ctx->ErrorOffset
						, fxsave_ctx->DataOffset
					);
				}
				else {
					log_printf(
						"\n"
						"FOP: %03hX\n"
						"FCS: %04hX FIP: 0x%08X\n"
						"FDS: %04hX FDP: 0x%08X"
						, fxsave_ctx->ErrorOpcode
						, fxsave_ctx->ErrorSelector, fxsave_ctx->ErrorOffset
						, fxsave_ctx->DataSelector, fxsave_ctx->DataOffset
					);
				}
			}

			uint8_t fpu_stack_top = (ctx->FloatSave.StatusWord >> 11) & 0b111;
			log_printf(
				"\n"
				"FCW: %04hX (%s, %s)\n"
				"FST: %04hX (%hhu %hhu %hhu %hhu)\n"
				"ST(0): %-5s %04hX %016llX\n"
				"ST(1): %-5s %04hX %016llX\n"
				"ST(2): %-5s %04hX %016llX\n"
				"ST(3): %-5s %04hX %016llX\n"
				"ST(4): %-5s %04hX %016llX\n"
				"ST(5): %-5s %04hX %016llX\n"
				"ST(6): %-5s %04hX %016llX\n"
				"ST(7): %-5s %04hX %016llX\n"
				, ctx->FloatSave.ControlWord, rounding_strings[((ctx->FloatSave.ControlWord >> 10) & 0b11)], fpu_precision_strings[((ctx->FloatSave.ControlWord >> 8) & 0b11)]
				, ctx->FloatSave.StatusWord, (ctx->FloatSave.StatusWord >> 8) & 0b1, (ctx->FloatSave.StatusWord >> 9) & 0b1, (ctx->FloatSave.StatusWord >> 10) & 0b1, (ctx->FloatSave.StatusWord >> 14) & 0b1
				, TagWordString(0), FPUReg(0)
				, TagWordString(1), FPUReg(1)
				, TagWordString(2), FPUReg(2)
				, TagWordString(3), FPUReg(3)
				, TagWordString(4), FPUReg(4)
				, TagWordString(5), FPUReg(5)
				, TagWordString(6), FPUReg(6)
				, TagWordString(7), FPUReg(7)
			);
		}
		if (exception_detail_level & ThExceptionDetailLevel::SSE) {
			XSAVE_FORMAT* fxsave_ctx = (XSAVE_FORMAT*)ctx->ExtendedRegisters;
			log_printf(
				"\n"
				"MXCSR: 0x%08X (%s)\n"
				"XMM0:  %016llX %016llX\n"
				"XMM1:  %016llX %016llX\n"
				"XMM2:  %016llX %016llX\n"
				"XMM3:  %016llX %016llX\n"
				"XMM4:  %016llX %016llX\n"
				"XMM5:  %016llX %016llX\n"
				"XMM6:  %016llX %016llX\n"
				"XMM7:  %016llX %016llX\n"
#ifdef TH_X64
				"XMM8:  %016llX %016llX\n"
				"XMM9:  %016llX %016llX\n"
				"XMM10: %016llX %016llX\n"
				"XMM11: %016llX %016llX\n"
				"XMM12: %016llX %016llX\n"
				"XMM13: %016llX %016llX\n"
				"XMM14: %016llX %016llX\n"
				"XMM15: %016llX %016llX\n"
#endif
				, fxsave_ctx->MxCsr, rounding_strings[(fxsave_ctx->MxCsr >> 13) & 0b11]
				, fxsave_ctx->XmmRegisters[0].High, fxsave_ctx->XmmRegisters[0].Low
				, fxsave_ctx->XmmRegisters[1].High, fxsave_ctx->XmmRegisters[1].Low
				, fxsave_ctx->XmmRegisters[2].High, fxsave_ctx->XmmRegisters[2].Low
				, fxsave_ctx->XmmRegisters[3].High, fxsave_ctx->XmmRegisters[3].Low
				, fxsave_ctx->XmmRegisters[4].High, fxsave_ctx->XmmRegisters[4].Low
				, fxsave_ctx->XmmRegisters[5].High, fxsave_ctx->XmmRegisters[5].Low
				, fxsave_ctx->XmmRegisters[6].High, fxsave_ctx->XmmRegisters[6].Low
				, fxsave_ctx->XmmRegisters[7].High, fxsave_ctx->XmmRegisters[7].Low
#ifdef TH_X64
				, fxsave_ctx->XmmRegisters[8].High, fxsave_ctx->XmmRegisters[8].Low
				, fxsave_ctx->XmmRegisters[9].High, fxsave_ctx->XmmRegisters[9].Low
				, fxsave_ctx->XmmRegisters[10].High, fxsave_ctx->XmmRegisters[10].Low
				, fxsave_ctx->XmmRegisters[11].High, fxsave_ctx->XmmRegisters[11].Low
				, fxsave_ctx->XmmRegisters[12].High, fxsave_ctx->XmmRegisters[12].Low
				, fxsave_ctx->XmmRegisters[13].High, fxsave_ctx->XmmRegisters[13].Low
				, fxsave_ctx->XmmRegisters[14].High, fxsave_ctx->XmmRegisters[14].Low
				, fxsave_ctx->XmmRegisters[15].High, fxsave_ctx->XmmRegisters[15].Low
#endif
			);
		}
		if (exception_detail_level & ThExceptionDetailLevel::Debug) {
			log_printf(
				"\n"
				"DB0: 0x%p DB1: 0x%p DB2: 0x%p DB3: 0x%p\n"
				"DB6: 0x%p DB7: 0x%p\n"
				, ctx->Dr0, ctx->Dr1, ctx->Dr2, ctx->Dr3
				, ctx->Dr6, ctx->Dr7
			);
		}
	}
}

static void log_print_rva_and_module(HMODULE mod, void *addr)
{
	size_t crash_fn_len = GetModuleFileNameU(mod, NULL, 0) + 1;
	VLA(char, crash_fn, crash_fn_len);
	if (GetModuleFileNameU(mod, crash_fn, crash_fn_len)) {
		log_printf(
			" (Rx%zX) (%s)",
			(uintptr_t)addr - (uintptr_t)mod,
			PathFindFileNameU(crash_fn)
		);
	}
	VLA_FREE(crash_fn);
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

	uintptr_t base;

#ifdef TH_X64
	if (lpER->NumberParameters < 4) {
		return nullptr;
	}
	base = lpER->ExceptionInformation[3];
#else
	if (lpER->NumberParameters < 3) {
		return nullptr;
	}
	base = 0;
#endif

	if (lpER->ExceptionCode != EH_EXCEPTION_NUMBER ||
		lpER->ExceptionInformation[0] != EH_MAGIC_NUMBER1) {
		return nullptr;
	}

	DWORD *throwInfo = (DWORD*)(base + lpER->ExceptionInformation[2]);
	if (throwInfo == nullptr) {
		return nullptr;
	}
	DWORD *catchableTypeArray = (DWORD*)(base + throwInfo[3]);
	if (catchableTypeArray[0] < 1) { // array size
		return nullptr;
	}
	DWORD *catchableType = (DWORD*)(base + catchableTypeArray[1]);
	void **typeDescriptor = (void**)(base + catchableType[1]);
	return (const char*)(&typeDescriptor[2]);
}

static TH_CALLER_FREE char* get_windows_error_message(DWORD ExceptionCode) {
	HMODULE ntdll_handle = GetModuleHandleW(L"ntdll.dll");
	HRSRC resource_info_handle = FindResource(ntdll_handle, MAKEINTRESOURCE(1), RT_MESSAGETABLE);
	HGLOBAL resource_handle = LoadResource(ntdll_handle, resource_info_handle);

	MESSAGE_RESOURCE_DATA* resource_pointer = (MESSAGE_RESOURCE_DATA*)LockResource(resource_handle);
	const size_t message_count = resource_pointer->NumberOfBlocks;

	MESSAGE_RESOURCE_ENTRY* message_entry = NULL;
	DWORD message_id;
	for (size_t i = 0; i < message_count; ++i) {
		MESSAGE_RESOURCE_BLOCK* message_block = &resource_pointer->Blocks[i];
		if ((ExceptionCode >= message_block->LowId) & (ExceptionCode <= message_block->HighId)) {
			message_id = message_block->LowId;
			message_entry = (MESSAGE_RESOURCE_ENTRY*)((uintptr_t)resource_pointer + message_block->OffsetToEntries);
			break;
		}
	}
	char* message_text = NULL;
	if (message_entry) {
		while (message_id < ExceptionCode) {
			++message_id;
			message_entry = (MESSAGE_RESOURCE_ENTRY*)((uintptr_t)message_entry + message_entry->Length);
		}
		bool message_is_utf16 = (message_entry->Flags == MESSAGE_RESOURCE_UNICODE);
		size_t message_length = (message_entry->Length - offsetof(MESSAGE_RESOURCE_ENTRY, Text)) >> (uint8_t)message_is_utf16;
		message_text = (char*)malloc(message_length);
		if (!message_is_utf16) {
			memcpy(message_text, message_entry->Text, message_length);
		}
		else {
			StringToUTF8(message_text, (wchar_t*)message_entry->Text, message_length);
		}
	}
	return message_text;
}

static void log_print_windows_error_message(LPEXCEPTION_RECORD lpER) {
	char* error_message = get_windows_error_message(lpER->ExceptionCode);
	str_ascii_replace(error_message, '\r', ' ');
	str_ascii_replace(error_message, '\n', ' ');
	switch (lpER->ExceptionCode) {
		case EXCEPTION_IN_PAGE_ERROR:
		case STATUS_ACCESS_VIOLATION: {
			static constexpr char* access_types[] = {
				/*EXCEPTION_READ_FAULT*/	"read",
				/*EXCEPTION_WRITE_FAULT*/	"written",
				NULL, NULL, NULL, NULL, NULL, NULL,
				/*EXCEPTION_EXECUTE_FAULT*/	"executed"
			};
			log_printf(error_message, lpER->ExceptionAddress, lpER->ExceptionInformation[1], access_types[lpER->ExceptionInformation[0]], lpER->ExceptionInformation[2]);
			break;
		}
		default:
			log_print(error_message);
			break;
	}
	free(error_message);
}

static void log_print_error_source(HMODULE crash_mod, void* address) {
	if (crash_mod) {
		log_print_rva_and_module(crash_mod, address);
	}
	else {
		HackpointMemoryName hackpoint_location = locate_address_in_stage_pages(address);
		if (hackpoint_location.name) {
			log_printf(" (\"%s\" + 0x%zu Stage %zu)", hackpoint_location.name, hackpoint_location.offset, hackpoint_location.stage_num);
		}
	}
	log_print("\n");
}

#define STATUS_NOT_IMPLEMENTED				((DWORD)0xC0000002L)
#define STATUS_INVALID_LOCK_SEQUENCE		((DWORD)0xC000001EL)
#define STATUS_BAD_STACK					((DWORD)0xC0000028L)
#define STATUS_INVALID_UNWIND_TARGET		((DWORD)0xC0000029L)
#define STATUS_BAD_FUNCTION_TABLE			((DWORD)0xC00000FFL)
#define STATUS_DATATYPE_MISALIGNMENT_ERROR	((DWORD)0xC00002C5L)
#define STATUS_HEAP_CORRUPTION				((DWORD)0xC0000374L)

LONG WINAPI exception_filter(LPEXCEPTION_POINTERS lpEI)
{
	LPEXCEPTION_RECORD lpER = lpEI->ExceptionRecord;
	
	switch(lpER->ExceptionCode) {
		// These should be all that matter. Unfortunately, we tend
		// to get way more than we want, particularly with wininet.
		case STATUS_ACCESS_VIOLATION:
		case STATUS_ILLEGAL_INSTRUCTION:
		case STATUS_INTEGER_DIVIDE_BY_ZERO:
		case EH_EXCEPTION_NUMBER:

		case STATUS_NOT_IMPLEMENTED:
		case STATUS_INVALID_LOCK_SEQUENCE:
		case STATUS_ARRAY_BOUNDS_EXCEEDED:
		case STATUS_PRIVILEGED_INSTRUCTION:
		case STATUS_DATATYPE_MISALIGNMENT_ERROR:
		case STATUS_ASSERTION_FAILURE:

		case STATUS_FLOAT_INVALID_OPERATION:
		case STATUS_FLOAT_OVERFLOW:
		case STATUS_FLOAT_STACK_CHECK:
		case STATUS_FLOAT_UNDERFLOW:
		case STATUS_FLOAT_MULTIPLE_FAULTS:
		case STATUS_FLOAT_MULTIPLE_TRAPS:

		case STATUS_STACK_OVERFLOW:
		case STATUS_STACK_BUFFER_OVERRUN:
		case STATUS_BAD_STACK:
		case STATUS_INVALID_UNWIND_TARGET:
		case STATUS_BAD_FUNCTION_TABLE:
		case STATUS_HEAP_CORRUPTION:
			break;

		case EXCEPTION_BREAKPOINT:
			if (!IsDebuggerPresent()) {
				break;
			}
			[[fallthrough]];
		default:
			return EXCEPTION_CONTINUE_SEARCH;
	}

	const char *cpp_name = get_cxx_eh_typename(lpER);

	// Only show Cn::XH exceptions once because on some PCs CoreUIComponents.dll
	// and CoreMessaging.dll throw a lot of those for whatever reason. See GH-129.
	// I have no clue what Cn::XH is, but it seems to be a wrapper around
	// System::Exception (again, no clue if that's related to CLR)
	if (cpp_name && !strcmp(cpp_name, ".?AUXH@Cn@@")) { // struct Cn::XH
		static LONG count = 0;
		if (1 == InterlockedCompareExchange(&count, 1, 0))
			return EXCEPTION_CONTINUE_SEARCH;
	}
	
	log_printf(
		"\n"
		"===\n"
		"Exception %08X at 0x%p",
		lpER->ExceptionCode, lpER->ExceptionAddress
	);

	HMODULE crash_mod = GetModuleContaining(lpER->ExceptionAddress);
	log_print_error_source(crash_mod, lpER->ExceptionAddress);

	log_print(
		"\n"
		"Description:\n"
	);

	if (!cpp_name) {
		log_print_windows_error_message(lpER);
	}
	else {
		log_printf("C++ EH type: %s", cpp_name);
	}
	log_print("\n");

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
		
		log_print("\nStack trace:\n");

		USHORT skip = 0;
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
		uintptr_t stop_at = (uintptr_t)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "KiUserExceptionDispatcher");
		for (USHORT i = skip; i < captured; i++) {
			if (((uintptr_t)trace[i] - stop_at) <= 16) {
				skip = i + 1;
				break;
			}
		}
		for(USHORT i = skip; i < captured; i++) {
			HMODULE mod = GetModuleContaining(trace[i]);
			log_printf("[%02u] 0x%p", captured - i, trace[i]);
			log_print_error_source(mod, trace[i]);
		}
	}
	log_print(
		"===\n"
		"\n"
	);
	return EXCEPTION_CONTINUE_SEARCH;
}
#pragma optimize("", on)

void exception_init(void)
{
	AddVectoredExceptionHandler(0, exception_filter);
}
