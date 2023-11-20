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

	ManualTrace	= 0b00100000, // FALSE = CaptureStackBackTrace, TRUE = Manual stack walk
	TraceDump	= 0b01000000, // Include byte dumps in manual trace

	Extra		= 0b10000000, // Segment data, x87 CS/DS

	Full		= 0b01111111, // All reasonable information
	Extreme		= 0b11111111  // All possible information
};

static uint8_t exception_detail_level = ThExceptionDetailLevel::Basic;

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
			};
			HANDLE current_thread_handle = GetCurrentThread();
			auto get_segment_data = [&](DWORD segment, LDT_ENTRY& entry, uintptr_t& base_out, size_t& limit_out) {
				GetThreadSelectorEntry(current_thread_handle, segment, &entry);
				base_out = entry.BaseLow | entry.HighWord.Bytes.BaseMid << 16 | entry.HighWord.Bytes.BaseHi << 24;
				limit_out = entry.LimitLow | entry.HighWord.Bits.LimitHi << 16;
				if (entry.HighWord.Bits.Granularity) {
					limit_out = limit_out << 12 | 0xFFF;
				}
			};
			LDT_ENTRY cs_seg_data, ss_seg_data, ds_seg_data, es_seg_data, fs_seg_data, gs_seg_data;
			uintptr_t cs_base, ss_base, ds_base, es_base, fs_base, gs_base;
			size_t cs_limit, ss_limit, ds_limit, es_limit, fs_limit, gs_limit;
			get_segment_data(ctx->SegCs, cs_seg_data, cs_base, cs_limit);
			get_segment_data(ctx->SegSs, ss_seg_data, ss_base, ss_limit);
			get_segment_data(ctx->SegDs, ds_seg_data, ds_base, ds_limit);
			get_segment_data(ctx->SegEs, es_seg_data, es_base, es_limit);
			get_segment_data(ctx->SegFs, fs_seg_data, fs_base, fs_limit);
			get_segment_data(ctx->SegGs, gs_seg_data, gs_base, gs_limit);
			if (exception_detail_level & ThExceptionDetailLevel::Extra) {
				log_printf(
					"\n"
					"CS: 0x%04hX (Base: %8X, Limit: %8X, Data: %s, Priv: %hhu, Type: %s%s)\n"
					"SS: 0x%04hX (Base: %8X, Limit: %8X, Data: %s, Priv: %hhu, Type: %s%s)\n"
					"DS: 0x%04hX (Base: %8X, Limit: %8X, Data: %s, Priv: %hhu, Type: %s%s)\n"
					"ES: 0x%04hX (Base: %8X, Limit: %8X, Data: %s, Priv: %hhu, Type: %s%s)\n"
					"FS: 0x%04hX (Base: %8X, Limit: %8X, Data: %s, Priv: %hhu, Type: %s%s)\n"
					"GS: 0x%04hX (Base: %8X, Limit: %8X, Data: %s, Priv: %hhu, Type: %s%s)\n"
					, ctx->SegCs, cs_base, cs_limit, (cs_seg_data.HighWord.Bits.Default_Big ? "32-bit" : (cs_seg_data.HighWord.Bits.Reserved_0 ? "64-bit" : "16-bit")), cs_seg_data.HighWord.Bits.Dpl, segment_types[cs_seg_data.HighWord.Bits.Type], (cs_seg_data.HighWord.Bits.Pres ? "" : " (Not Present)")
					, ctx->SegSs, ss_base, ss_limit, (ss_seg_data.HighWord.Bits.Default_Big ? "32-bit" : (ss_seg_data.HighWord.Bits.Reserved_0 ? "64-bit" : "16-bit")), ss_seg_data.HighWord.Bits.Dpl, segment_types[ss_seg_data.HighWord.Bits.Type], (ss_seg_data.HighWord.Bits.Pres ? "" : " (Not Present)")
					, ctx->SegDs, ds_base, ds_limit, (ds_seg_data.HighWord.Bits.Default_Big ? "32-bit" : (ds_seg_data.HighWord.Bits.Reserved_0 ? "64-bit" : "16-bit")), ds_seg_data.HighWord.Bits.Dpl, segment_types[ds_seg_data.HighWord.Bits.Type], (ds_seg_data.HighWord.Bits.Pres ? "" : " (Not Present)")
					, ctx->SegEs, es_base, es_limit, (es_seg_data.HighWord.Bits.Default_Big ? "32-bit" : (es_seg_data.HighWord.Bits.Reserved_0 ? "64-bit" : "16-bit")), es_seg_data.HighWord.Bits.Dpl, segment_types[es_seg_data.HighWord.Bits.Type], (es_seg_data.HighWord.Bits.Pres ? "" : " (Not Present)")
					, ctx->SegFs, fs_base, fs_limit, (fs_seg_data.HighWord.Bits.Default_Big ? "32-bit" : (fs_seg_data.HighWord.Bits.Reserved_0 ? "64-bit" : "16-bit")), fs_seg_data.HighWord.Bits.Dpl, segment_types[fs_seg_data.HighWord.Bits.Type], (fs_seg_data.HighWord.Bits.Pres ? "" : " (Not Present)")
					, ctx->SegGs, gs_base, gs_limit, (gs_seg_data.HighWord.Bits.Default_Big ? "32-bit" : (gs_seg_data.HighWord.Bits.Reserved_0 ? "64-bit" : "16-bit")), gs_seg_data.HighWord.Bits.Dpl, segment_types[gs_seg_data.HighWord.Bits.Type], (gs_seg_data.HighWord.Bits.Pres ? "" : " (Not Present)")
				);
			}
			else {
				log_printf(
					"\n"
					"CS: 0x%04hX %X (%X)\n"
					"SS: 0x%04hX %X (%X)\n"
					"DS: 0x%04hX %X (%X)\n"
					"ES: 0x%04hX %X (%X)\n"
					"FS: 0x%04hX %X (%X)\n"
					"GS: 0x%04hX %X (%X)\n"
					, ctx->SegCs, cs_base, cs_limit
					, ctx->SegSs, ss_base, ss_limit
					, ctx->SegDs, ds_base, ds_limit
					, ctx->SegEs, es_base, es_limit
					, ctx->SegFs, fs_base, fs_limit
					, ctx->SegGs, gs_base, gs_limit
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

TH_CALLER_FREE static char* get_windows_error_message(DWORD ExceptionCode) {
	HMODULE ntdll_handle = GetModuleHandleW(L"ntdll.dll");
	HRSRC resource_info_handle = FindResourceEx(ntdll_handle, RT_MESSAGETABLE, MAKEINTRESOURCE(1), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
	HGLOBAL resource_handle = LoadResource(ntdll_handle, resource_info_handle);

	MESSAGE_RESOURCE_DATA* resource_pointer = (MESSAGE_RESOURCE_DATA*)LockResource(resource_handle);
	if unexpected(resource_pointer == NULL) {
		// TODO Figure out why Wine isn't finding the message table resource in ntdll
		return NULL;
	}
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
	if (char* error_message = get_windows_error_message(lpER->ExceptionCode)) {
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
	else {
		log_print("No error description available.");
	}
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
}

void manual_stack_walk(uintptr_t current_esp) {
#ifdef TH_X64
	NT_TIB* tib = (NT_TIB*)__readgsqword(offsetof(NT_TIB, Self));
#else
	NT_TIB* tib = (NT_TIB*)__readfsdword(offsetof(NT_TIB, Self));
#endif
	
#ifdef TH_X64
#define StkPtrReg "RSP"
#define InsPtrReg "RIP"
#else
#define StkPtrReg "ESP"
#define InsPtrReg "EIP"
#endif

	uintptr_t* stack_top = (uintptr_t*)tib->StackBase;
	uintptr_t* stack_bottom = (uintptr_t*)tib->StackLimit;
	
	if (!(current_esp < (uintptr_t)stack_top && current_esp >= (uintptr_t)stack_bottom)) {
		log_printf(
			"\n"
			"Stack walk ERROR: " StkPtrReg " is not within the stack bounds specified by the TEB! (0x%p->0x%p)\n"
			, stack_bottom, stack_top
		);
		return;
	}

	log_printf(
		"\n"
		"Stack walk: (0x%p->0x%p)\n"
		, current_esp, stack_top
	);

#ifdef TH_X64
	if (current_esp % 16) {
#else
	if (((uintptr_t)stack_top - current_esp) % sizeof(uintptr_t)) {
#endif
		log_print(
			"WARNING: Stack is not aligned, data may be unreliable.\n"
		);
	}
	MEMORY_BASIC_INFORMATION mbi;
	enum PtrState_TypeVals : uint8_t {
		RawValue = 0,
		PossiblePointer = 1,
		ReturnAddrIndirect2 = 2,
		ReturnAddrIndirect3 = 3,
		ReturnAddrIndirect4 = 4,
		ReturnAddr = 5,
		ReturnAddrIndirect6 = 6,
		ReturnAddrIndirect7 = 7,
		FarReturnAddrIndirect2 = 10,
		FarReturnAddrIndirect3 = 11,
		FarReturnAddrIndirect4 = 12,
		FarReturnAddr = 13, // Not valid in x64
		FarReturnAddrIndirect6 = 14,
		FarReturnAddrIndirect7 = 15
	};
	enum PtrState_AccessVals : uint8_t {
		None = 0,
		ReadOnly = 1,
		WriteCopy = 2,
		ReadWrite = 3,
		Execute = 4,
		ExecuteRead = 5,
		ExecuteWriteCopy = 6,
		ExecuteReadWrite = 7,
	};
	static constexpr const char* AccessStrings[] = {
		"(No Access)",
		"(Read Only)",
		"(Write-Copy)",
		"(Read Write)",
		"(Execute Only)",
		"(Execute Read)",
		"(Execute Write-Copy)",
		"(Execute Read Write)"
	};
	uint8_t stack_offset_length = snprintf(NULL, 0, "%X", (uintptr_t)(stack_top - 1) - current_esp);
	for (
		uintptr_t* stack_addr = (uintptr_t*)current_esp;
		stack_addr < stack_top - 1; // These don't necessarily align cleanly, so array indexing would be tricky to use here
		++stack_addr
	) {
		uintptr_t stack_value = *stack_addr;
		
		uint8_t ptr_value_type = RawValue;
		uint8_t ptr_access_type = None;
		bool ptr_on_stack = false;
		bool has_segment_override = false;
#ifdef TH_X64
		bool has_rex_byte = false;
#endif
		if (stack_value < 65536) { // Thin out values that can't ever be a valid pointer
			if (!(exception_detail_level & ThExceptionDetailLevel::TraceDump)) {
				// Memory protection hasn't been modified, so just continue the loop
				continue;	
			}
			ptr_value_type = RawValue;
		} else {
			// Is this a pointer to other stack data?
			if (stack_value < (uintptr_t)stack_top && stack_value >= (uintptr_t)current_esp) {
				ptr_on_stack = true;
			}
			// Is this a pointer to meaningful memory?
			if (VirtualQuery((void*)stack_value, &mbi, sizeof(MEMORY_BASIC_INFORMATION))) {
				switch (mbi.Protect & 0xFF) {
					default: case PAGE_NOACCESS: // ???
						ptr_access_type = None;
						break;
					case PAGE_READONLY: // Probably a pointer to a constant
						ptr_access_type = ReadOnly;
						break;
					case PAGE_WRITECOPY:
						if (VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_READONLY, &mbi.Protect)) {
							ptr_access_type = WriteCopy;
						} else {
							ptr_access_type = None;
						}
						break;
					case PAGE_READWRITE: // Probably a pointer to data
						ptr_access_type = ReadWrite;
						break;
					case PAGE_EXECUTE:
						if (VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_READONLY, &mbi.Protect)) {
							ptr_access_type = Execute;
						} else {
							ptr_access_type = None;
						}
						break;
					case PAGE_EXECUTE_READ: // Probably a return address
						ptr_access_type = ExecuteRead;
						break;
					case PAGE_EXECUTE_WRITECOPY: // Probably a return address, but we can't read it
						if (VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_READONLY, &mbi.Protect)) {
							ptr_access_type = ExecuteWriteCopy;
						} else {
							ptr_access_type = None;
						}
						break;
					case PAGE_EXECUTE_READWRITE: // Probably a codecave
						ptr_access_type = ExecuteReadWrite;
						break;
				}
			}
			switch (ptr_access_type) {
				default:
					TH_UNREACHABLE;
				// I draw the line at repeated prefixes, 16-bit calls, and 16-bit addressing. Screw that.
				case Execute: case ExecuteRead: case ExecuteWriteCopy: case ExecuteReadWrite:
					switch (stack_value - (uintptr_t)mbi.BaseAddress) {
						default:
#ifdef TH_X64
							switch (*(uint8_t*)(stack_value - 9)) {
								case 0x26: case 0x2E: case 0x36: case 0x3E: case 0x64: case 0x65: // ES, CS, SS, DS, FS, GS
									has_segment_override = true;
									break;
							}
							TH_FALLTHROUGH;
						case 8:
#endif
							switch (*(uint8_t*)(stack_value - 8)) {
#ifdef TH_X64
								case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
									has_rex_byte = true;
									break;
#endif
								case 0x26: case 0x2E: case 0x36: case 0x3E: case 0x64: case 0x65: // ES, CS, SS, DS, FS, GS
									has_segment_override = true;
									break;
							}
							TH_FALLTHROUGH;
						case 7:
							switch (*(uint8_t*)(stack_value - 7)) {
								case 0xFF: { // CALL Absolute Indirect
									uint8_t mod_byte = *(uint8_t*)(stack_value - 6);
									bool ptr_is_far = mod_byte != (mod_byte & 0xF7);
									mod_byte &= 0xF7;
									// Only ModRM 14 and 94 can be encoded as 7 bytes long
									switch (mod_byte) {
										case 0x14: // [disp32] (SIB)
											// ModRM 14 can only be 7 bytes long with SIB base == 5
											if ((*(uint8_t*)(stack_value - 5) & 0x7) != 5) break;
											TH_FALLTHROUGH;
										case 0x94: // [reg+disp32] (SIB)
											ptr_value_type = !ptr_is_far ? ReturnAddrIndirect7 : FarReturnAddrIndirect7;
											goto IdentifiedValueType;
									}
								}
									TH_FALLTHROUGH;
								default:
									has_segment_override = false;
#ifdef TH_X64
									has_rex_byte = false;
#endif
									break;
#ifdef TH_X64
								case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
									has_rex_byte = true;
									break;
#else
								case 0x9A: // FAR CALL Absolute Direct
									ptr_value_type = FarReturnAddr;
									goto IdentifiedValueType;
#endif
								case 0x26: case 0x2E: case 0x36: case 0x3E: case 0x64: case 0x65: // ES, CS, SS, DS, FS, GS
									has_segment_override = true;
#ifdef TH_X64
									has_rex_byte = false;
#endif
									break;
							}
							TH_FALLTHROUGH;
						case 6:
							switch (*(uint8_t*)(stack_value - 6)) { // CALL Absolute Indirect
								case 0xFF: {
									uint8_t mod_byte = *(uint8_t*)(stack_value - 5);
									bool ptr_is_far = mod_byte != (mod_byte & 0xF7);
									mod_byte &= 0xF7;
									// Only 15, 90-93, and 95-97 can be encoded as 6 bytes long
									if (mod_byte == 0x15 || // [disp32] (No SIB)
										(mod_byte & 0xF8) == 0x90 && (mod_byte & 0x7) != 4 // [reg+disp32] (No SIB)
									) {
										ptr_value_type = !ptr_is_far ? ReturnAddrIndirect6 : FarReturnAddrIndirect6;
										goto IdentifiedValueType;
									}
								}
									TH_FALLTHROUGH;
								default:
									has_segment_override = false;
#ifdef TH_X64
									has_rex_byte = false;
#endif
									break;
#ifdef TH_X64
								case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
									has_rex_byte = true;
									break;
#endif
								case 0x26: case 0x2E: case 0x36: case 0x3E: case 0x64: case 0x65: // ES, CS, SS, DS, FS, GS
									has_segment_override = true;
#ifdef TH_X64
									has_rex_byte = false;
#endif
									break;
							}
							TH_FALLTHROUGH;
						case 5:
							switch (*(uint8_t*)(stack_value - 5)) { // CALL Relative Displacement
								case 0xE8:
									ptr_value_type = ReturnAddr;
									goto IdentifiedValueType;
								default:
									has_segment_override = false;
#ifdef TH_X64
									has_rex_byte = false;
#endif
									break;
#ifdef TH_X64
								case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
									has_rex_byte = true;
									break;
#endif
								case 0x26: case 0x2E: case 0x36: case 0x3E: case 0x64: case 0x65: // ES, CS, SS, DS, FS, GS
									has_segment_override = true;
#ifdef TH_X64
									has_rex_byte = false;
#endif
									break;
							}
							TH_FALLTHROUGH;
						case 4:
							switch (*(uint8_t*)(stack_value - 4)) { // CALL Absolute Indirect
								case 0xFF: {
									uint8_t mod_byte = *(uint8_t*)(stack_value - 3);
									bool ptr_is_far = mod_byte != (mod_byte & 0xF7);
									mod_byte &= 0xF7;
									// Only 54 can be encoded as 4 bytes long
									if (mod_byte == 0x54) { // [ESP+disp8] (SIB)
										ptr_value_type = !ptr_is_far ? ReturnAddrIndirect4 : FarReturnAddrIndirect4;
										goto IdentifiedValueType;
									}
								}
									TH_FALLTHROUGH;
								default:
									has_segment_override = false;
#ifdef TH_X64
									has_rex_byte = false;
#endif
									break;
#ifdef TH_X64
								case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
									has_rex_byte = true;
									break;
#endif
								case 0x26: case 0x2E: case 0x36: case 0x3E: case 0x64: case 0x65: // ES, CS, SS, DS, FS, GS
									has_segment_override = true;
#ifdef TH_X64
									has_rex_byte = false;
#endif
									break;
							}
							TH_FALLTHROUGH;
						case 3:
							switch (*(uint8_t*)(stack_value - 3)) { // CALL Absolute Indirect
								case 0xFF: {
									uint8_t mod_byte = *(uint8_t*)(stack_value - 2);
									bool ptr_is_far = mod_byte != (mod_byte & 0xF7);
									mod_byte &= 0xF7;
									// Only 14, 50-53, and 55-57 can be encoded as 3 bytes long
									switch (mod_byte) {
										case 0x14: // [reg] (SIB)
											// ModRM 14 can only be 3 bytes long with SIB base != 5
											if ((*(uint8_t*)(stack_value - 1) & 0x7) == 5) break;
											TH_FALLTHROUGH;
										case 0x50: case 0x51: case 0x52: case 0x53: case 0x55: case 0x56: case 0x57: // [reg+disp8] (No SIB)
											ptr_value_type = !ptr_is_far ? ReturnAddrIndirect3 : FarReturnAddrIndirect3;
											goto IdentifiedValueType;
									}
								}
									TH_FALLTHROUGH;
								default:
									has_segment_override = false;
#ifdef TH_X64
									has_rex_byte = false;
#endif
									break;
#ifdef TH_X64
								case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
									has_rex_byte = true;
									break;
#endif
								case 0x26: case 0x2E: case 0x36: case 0x3E: case 0x64: case 0x65: // ES, CS, SS, DS, FS, GS
									has_segment_override = true;
#ifdef TH_X64
									has_rex_byte = false;
#endif
									break;
							}
							TH_FALLTHROUGH;
						case 2:
							if (*(uint8_t*)(stack_value - 2) == 0xFF) { // CALL Absolute Indirect
								uint8_t mod_byte = *(uint8_t*)(stack_value - 1);
								bool ptr_is_far = mod_byte != (mod_byte & 0xF7);
								mod_byte &= 0xF7;
								// Only 10-13, 16, 17, and D0-D7 can be encoded as 2 bytes long
								switch (mod_byte & 0xF8) {
									case 0x10: // [reg] (No SIB)
										if ((mod_byte & 0x6) == 0x4) break; // Filter out [reg] (SIB) and [disp32]
										TH_FALLTHROUGH;
									case 0xD0: // reg
										ptr_value_type = !ptr_is_far ? ReturnAddrIndirect2 : FarReturnAddrIndirect2;
										goto IdentifiedValueType;
								}
							}
							TH_FALLTHROUGH;
						case 1: case 0:
							break;
					}
					TH_FALLTHROUGH;
				case ReadOnly: case WriteCopy: case ReadWrite:
					if (VirtualCheckRegion((void*)stack_value, 16)) {
						ptr_value_type = PossiblePointer;
					}
					else {
						TH_FALLTHROUGH;
				case None:
						ptr_value_type = RawValue;
					}
					if (!(exception_detail_level & ThExceptionDetailLevel::TraceDump)) {
						// Memory protection might have been modified, so jump to the end where it's set back
						goto SkipPrint;
					}
					break;
			}
		}
	IdentifiedValueType:
		switch (ptr_value_type) {
			default:
				TH_UNREACHABLE;
			case RawValue:
				log_printf(
					"[" StkPtrReg "+%*X]\t%p"
					, stack_offset_length, (uintptr_t)stack_addr - current_esp, stack_value
				);
				break;
			case PossiblePointer: {
				log_printf(
					"[" StkPtrReg "+%*X]\t%p\t"
					, stack_offset_length, (uintptr_t)stack_addr - current_esp, stack_value
				);
				if (exception_detail_level & ThExceptionDetailLevel::ManualTrace) {
					log_printf(
						"DUMP %08X %08X %08X %08X "
						, _byteswap_ulong(((uint32_t*)stack_value)[0])
						, _byteswap_ulong(((uint32_t*)stack_value)[1])
						, _byteswap_ulong(((uint32_t*)stack_value)[2])
						, _byteswap_ulong(((uint32_t*)stack_value)[3])
					);
				}
				log_printf(
					"%s"
					, ptr_on_stack ? "(Stack)" : AccessStrings[ptr_access_type]
				);
				if (!ptr_on_stack) {
					log_print_error_source(GetModuleContaining((void*)stack_value), (void*)stack_value);
				}
				break;
			}
			case ReturnAddr: {
				uintptr_t call_dest_addr = (intptr_t)stack_value + *(int32_t*)(stack_value - 4);
				if (stack_value != call_dest_addr) {
					log_printf(
						"[" StkPtrReg "+%*X]\t%p\tRETURN to %p"
						, stack_offset_length, (uintptr_t)stack_addr - current_esp, stack_value
						, stack_value
					);
					log_print_error_source(GetModuleContaining((void*)stack_value), (void*)stack_value);
					log_printf(
						" from func %p"
						, call_dest_addr
					);
					log_print_error_source(GetModuleContaining((void*)call_dest_addr), (void*)call_dest_addr);
				} else {
#ifdef TH_X64
					uintptr_t call_addr = stack_value - (5 + has_segment_override + has_rex_byte);
#else
					uintptr_t call_addr = stack_value - (5 + has_segment_override);
#endif
					log_printf(
						"[" StkPtrReg "+%*X]\t%p\t" InsPtrReg " pushed by CALL 0 at %p"
						, stack_offset_length, (uintptr_t)stack_addr - current_esp, stack_value
						, call_addr
					);
					log_print_error_source(GetModuleContaining((void*)call_addr), (void*)call_addr);
				}
				break;
			}
			case ReturnAddrIndirect2: case ReturnAddrIndirect3: case ReturnAddrIndirect4: case ReturnAddrIndirect6: case ReturnAddrIndirect7: {
				log_printf(
					"[" StkPtrReg "+%*X]\t%p\tRETURN to %p"
					, stack_offset_length, (uintptr_t)stack_addr - current_esp, stack_value
					, stack_value
				);
				log_print_error_source(GetModuleContaining((void*)stack_value), (void*)stack_value);
#ifdef TH_X64
				uintptr_t call_addr = stack_value - (ptr_value_type + has_segment_override + has_rex_byte);
#else
				uintptr_t call_addr = stack_value - (ptr_value_type + has_segment_override);
#endif
				log_printf(
					" (Indirect call from %p"
					, call_addr
				);
				log_print_error_source(GetModuleContaining((void*)call_addr), (void*)call_addr);
				log_print(")");
				break;
			}
#ifndef TH_X64
			case FarReturnAddr: {
				log_printf(
					"[" StkPtrReg "+%*X]\t%p\tFAR RETURN to %04hX:%p"
					, stack_offset_length, (uintptr_t)stack_addr - current_esp, stack_value
					, *(uint16_t*)(stack_addr + 1), stack_value
				);
				log_print_error_source(GetModuleContaining((void*)stack_value), (void*)stack_value);
				uintptr_t call_dest_addr = *(uintptr_t*)(stack_value - 6);
				log_printf(
					" from func %04hX:%p"
					, *(uint16_t*)(stack_value - 2), call_dest_addr
				);
				log_print_error_source(GetModuleContaining((void*)call_dest_addr), (void*)call_dest_addr);
				break;
			}
#endif
			case FarReturnAddrIndirect2: case FarReturnAddrIndirect3: case FarReturnAddrIndirect4: case FarReturnAddrIndirect6: case FarReturnAddrIndirect7: {
				log_printf(
					"[" StkPtrReg "+%*X]\t%p\tFAR RETURN to %04hX:%p"
					, stack_offset_length, (uintptr_t)stack_addr - current_esp, stack_value
					, *(uint16_t*)(stack_addr + 1), stack_value
				);
				log_print_error_source(GetModuleContaining((void*)stack_value), (void*)stack_value);
#ifdef TH_X64
				uintptr_t call_addr = stack_value - ((ptr_value_type - 8) + has_segment_override + has_rex_byte);
#else
				uintptr_t call_addr = stack_value - ((ptr_value_type - 8) + has_segment_override);
#endif
				log_printf(
					" (Indirect call from %p"
					, call_addr
				);
				log_print_error_source(GetModuleContaining((void*)call_addr), (void*)call_addr);
				log_print(")");
			}
		}
		log_print("\n");
	SkipPrint:
		switch (ptr_access_type) {
			case WriteCopy: case Execute: case ExecuteWriteCopy:
				VirtualProtect(mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &mbi.Protect);
		}
	}
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

	if (exception_detail_level & (ThExceptionDetailLevel::ManualTrace | ThExceptionDetailLevel::TraceDump)) {
#ifdef TH_X64
		manual_stack_walk(lpEI->ContextRecord->Rsp);
#else
		manual_stack_walk(lpEI->ContextRecord->Esp);
#endif
	} else {
		// "Windows Server 2003 and Windows XP: The sum of the FramesToSkip
		// and FramesToCapture parameters must be less than 63."
		// https://msdn.microsoft.com/en-us/library/windows/desktop/bb204633(v=vs.85).aspx
		const DWORD frames_to_skip = 1;
		void *trace[62 - frames_to_skip];

		USHORT captured = CaptureStackBackTrace(
			frames_to_skip, elementsof(trace), trace, nullptr
		);
		if (captured) {

			log_print("\nStack trace:\n");

			USHORT skip = 0;
			if (crash_mod) {
				while (
					GetModuleContaining(trace[skip]) != crash_mod
					&& skip < captured
					) {
					skip++;
				}
			}
			if (skip == captured) {
				skip = 0;
			}
			uintptr_t stop_at = (uintptr_t)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "KiUserExceptionDispatcher");
			for (USHORT i = skip; i < captured; i++) {
				if (((uintptr_t)trace[i] - stop_at) <= 16) {
					skip = i + 1;
					break;
				}
			}
			for (USHORT i = skip; i < captured; i++) {
				HMODULE mod = GetModuleContaining(trace[i]);
				log_printf("[%02u] 0x%p", captured - i, trace[i]);
				log_print_error_source(mod, trace[i]);
				log_print("\n");
			}
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

void exception_load_config()
{
	exception_detail_level = (uint8_t)globalconfig_get_integer("exception_detail", ThExceptionDetailLevel::Basic);
}
