/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Expression parsing
  */

#include "thcrap.h"
#include <intrin.h>

#undef stricmp
#define stricmp ascii_stricmp
#undef strnicmp
#define strnicmp ascii_strnicmp

//#define EnableExpressionLogging

#ifdef EnableExpressionLogging
#define ExpressionLogging(...) log_printf(__VA_ARGS__)
#else
// Using __noop makes the compiler check the validity of
// the macro contents without actually compiling them.
#define ExpressionLogging(...) TH_EVAL_NOOP(__VA_ARGS__)
#endif

enum ManufacturerID : int8_t {
	Unknown = -1,
	AMD = 0,
	Intel = 1
};

union FamilyData_t {
	uint32_t raw;
	struct {
		uint32_t stepping : 4;
		uint32_t model : 8;
		uint32_t family : 8;
		uint32_t zero : 12;
	};
};

union WinVersion_t {
	uint32_t raw;
	struct {
		uint8_t service_pack_minor;
		uint8_t service_pack_major;
		uint8_t version_minor;
		uint8_t version_major;
	};
};

static const char* wine_version = NULL;
struct CPUID_Data_t {
	BOOL OSIsX64 = false;
	WinVersion_t WindowsVersion = { 0 };
	FamilyData_t FamilyData = { 0 };
	ManufacturerID Manufacturer = Unknown;
	// TODO: Core count detection for omitting LOCK?
	// Hopefully 64 bytes is a reasonable default for anything ancient enough not to report this
	uint32_t cache_line_size = 64;
	// TODO: Add some builtin code options for generating fxsave/xsave patterns
	uint32_t xsave_mask_low = 0;
	uint32_t xsave_mask_high = 0;
	size_t xsave_max_size = 512; // Default to returning the FXSAVE buffer size incase XSAVE isn't present
	bool xsave_restores_fpu_errors = true; // Intel doesn't have this flag or any way of disabling this AFAIK, so default to true
	// TODO: Figure out how to deal with Intel's Alder Lake BS
	// Apparently individual cores have unique CPUID values,
	// but that hasn't mattered before since all cores have been the same.
	bool hybrid_architecture = false;
	struct {
		bool HasTSC = false;
		bool HasCMPXCHG8 = false;
		bool HasSYSENTER = false;
		bool HasCMOV = false;
		bool HasCLFLUSH = false;
		bool HasMMX = false;
		bool HasFXSAVE = false;
		bool HasSSE = false;
		bool HasSSE2 = false;
		bool HasSSE3 = false;
		bool HasPCLMULQDQ = false;
		bool HasSSSE3 = false;
		bool HasFMA = false;
		bool HasCMPXCHG16B = false;
		bool HasSSE41 = false;
		bool HasSSE42 = false;
		bool HasMOVBE = false;
		bool HasPOPCNT = false;
		bool HasAES = false;
		bool HasXSAVE = false;
		bool HasAVX = false;
		bool HasF16C = false;
		bool HasRDRAND = false;
		bool HasFSGSBASE = false;
		bool HasBMI1 = false;
		bool HasTSXHLE = false;
		bool HasAVX2 = false;
		bool FDP_EXCPTN_ONLY = false;
		bool HasBMI2 = false;
		bool HasERMS = false;
		bool HasTSXRTM = false;
		bool FCS_FDS_DEP = false;
		bool HasMPX = false;
		bool HasAVX512F = false;
		bool HasAVX512DQ = false;
		bool HasRDSEED = false;
		bool HasADX = false;
		bool HasAVX512IFMA = false;
		bool HasCLFLUSHOPT = false;
		bool HasCLWB = false;
		bool HasAVX512PF = false;
		bool HasAVX512ER = false;
		bool HasAVX512CD = false;
		bool HasSHA = false;
		bool HasAVX512BW = false;
		bool HasAVX512VL = false;
		bool HasPREFETCHWT1 = false;
		bool HasAVX512VBMI = false;
		bool HasPKU = false;
		bool HasWAITPKG = false;
		bool HasAVX512VBMI2 = false;
		bool HasSHSTK = false;
		bool HasGFNI = false;
		bool HasVAES = false;
		bool HasVPCLMULQDQ = false;
		bool HasAVX512VNNI = false;
		bool HasAVX512BITALG = false;
		bool HasAVX512VPOPCNTDQ = false;
		bool HasRDPID = false;
		bool HasCLDEMOTE = false;
		bool HasMOVDIRI = false;
		bool HasMOVDIR64B = false;
		bool HasAVX5124VNNIW = false;
		bool HasAVX5124FMAPS = false;
		bool HasFSRM = false;
		bool HasUINTR = false;
		bool HasAVX512VP2I = false;
		bool HasSERIALIZE = false;
		bool HasCET = false;
		bool HasAMXBF16 = false;
		bool HasAVX512FP16 = false;
		bool HasAMXTILE = false;
		bool HasAMXINT8 = false;
		bool HasSHA512 = false;
		bool HasSM3 = false;
		bool HasSM4 = false;
		bool HasRAOINT = false;
		bool HasAVXVNNI = false;
		bool HasAVX512BF16 = false;
		bool HasCMPCCXADD = false;
		bool HasFRMB0 = false;
		bool HasFRSB = false;
		bool HasFRCSB = false;
		bool HasAMXFP16 = false;
		bool HasAVXIFMA = false;
		bool HasAVXVNNIINT8 = false;
		bool HasAVXNECONVERT = false;
		bool HasAMXCOMPLEX = false;
		bool HasAVXVNNIINT16 = false;
		bool HasPREFETCHI = false;
		bool HasAVX10 = false;
		bool HasAPXF = false;
		bool HasXSAVEOPT = false;
		bool HasXSAVEC = false;
		bool HasSYSCALL = false;
		bool HasMMXEXT = false;
		bool HasRDTSCP = false;
		bool Has3DNOWEXT = false;
		bool Has3DNOW = false;
		bool HasLMLSAHF = false;
		bool HasABM = false;
		bool HasSSE4A = false;
		bool HasMXCSRMM = false;
		bool HasPREFETCHW = false;
		bool HasXOP = false;
		bool HasLWP = false;
		bool HasFMA4 = false;
		bool HasTBM = false;
		bool HasMONITORX = false;
		bool HasCLZERO = false;
		bool HasRDPRU = false;
		bool HasMCOMMIT = false;
		bool HasLWPVAL = false;
		bool HasMVEX = false;
	};
	CPUID_Data_t(void) {
		// GetProcAddress is used to be compatible with XP SP1.
		// https://docs.microsoft.com/en-us/windows/win32/api/wow64apiset/nf-wow64apiset-iswow64process
		if (auto IsWow64ProcessVar = (decltype(&IsWow64Process))GetProcAddress(GetModuleHandleA("Kernel32.dll"), "IsWow64Process")) {
			BOOL IsX64;
			if (IsWow64ProcessVar(GetCurrentProcess(), &IsX64)) {
				this->OSIsX64 = IsX64;
			}
		}

		if (const char* (TH_CDECL * pwine_get_version)(void) = (decltype(pwine_get_version))GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version")) {
			wine_version = pwine_get_version();
		}

		PEB* peb = CurrentPeb();

		WinVersion_t windows_version;
		windows_version.version_major = (uint8_t)peb->OSMajorVersion;
		windows_version.version_minor = (uint8_t)peb->OSMinorVersion;
		windows_version.service_pack_major = peb->OSCSDMajorVersion;
		windows_version.service_pack_minor = peb->OSCSDMinorVersion;
		this->WindowsVersion = windows_version;

		int data[4];
		// data[0]	EAX
		// data[1]	EBX
		// data[2]	ECX
		// data[3]	EDX
		__cpuid(data, 0);
		if (data[1] == TextInt('G', 'e', 'n', 'u') &&
			data[3] == TextInt('i', 'n', 'e', 'I') &&
			data[2] == TextInt('n', 't', 'e', 'l')) {
				Manufacturer = Intel;
		}
		else if (data[1] == TextInt('A', 'u', 't', 'h') &&
				 data[3] == TextInt('e', 'n', 't', 'i') &&
				 data[2] == TextInt('c', 'A', 'M', 'D')) {
					Manufacturer = AMD;
		}
		else {
			Manufacturer = Unknown;
		}
		const uint32_t& data0 = data[0]; // EAX
		const uint32_t& data1 = data[1]; // EBX
		const uint32_t& data2 = data[2]; // ECX
		const uint32_t& data3 = data[3]; // EDX
		switch (data[0]) {
			default: //case 13:
				__cpuidex(data, 13, 0);
				xsave_mask_low = data[0];
				xsave_mask_high = data[3];
				xsave_max_size = data[2];
				__cpuidex(data, 13, 1);
				HasXSAVEOPT			= bittest32(data[0], 0);
				HasXSAVEC			= bittest32(data[0], 1);
			case 12: case 11: case 10: case 9: case 8: case 7:
				__cpuidex(data, 7, 0);
				HasFSGSBASE			= bittest32(data[1], 0);
				HasBMI1				= bittest32(data[1], 3);
				HasTSXHLE			= bittest32(data[1], 4);
				HasAVX2				= bittest32(data[1], 5);
				FDP_EXCPTN_ONLY		= bittest32(data[1], 6);
				HasBMI2				= bittest32(data[1], 8);
				HasERMS				= bittest32(data[1], 9);
				HasTSXRTM			= bittest32(data[1], 11);
				FCS_FDS_DEP			= bittest32(data[1], 13);
				HasMPX				= bittest32(data[1], 14);
				HasAVX512F			= bittest32(data[1], 16);
				HasAVX512DQ			= bittest32(data[1], 17);
				HasRDSEED			= bittest32(data[1], 18);
				HasADX				= bittest32(data[1], 19);
				HasAVX512IFMA		= bittest32(data[1], 21);
				HasCLFLUSHOPT		= bittest32(data[1], 23);
				HasCLWB				= bittest32(data[1], 24);
				HasAVX512PF			= bittest32(data[1], 26);
				HasAVX512ER			= bittest32(data[1], 27);
				HasAVX512CD			= bittest32(data[1], 28);
				HasSHA				= bittest32(data[1], 29);
				HasAVX512BW			= bittest32(data[1], 30);
				HasAVX512VL			= bittest32(data[1], 31);
				HasPREFETCHWT1		= bittest32(data[2], 0);
				HasAVX512VBMI		= bittest32(data[2], 1);
				HasPKU				= bittest32(data[2], 3) & bittest32(data[2], 4);
				HasWAITPKG			= bittest32(data[2], 5);
				HasAVX512VBMI2		= bittest32(data[2], 6);
				HasSHSTK			= bittest32(data[2], 7);
				HasGFNI				= bittest32(data[2], 8);
				HasVAES				= bittest32(data[2], 9);
				HasVPCLMULQDQ		= bittest32(data[2], 10);
				HasAVX512VNNI		= bittest32(data[2], 11);
				HasAVX512BITALG		= bittest32(data[2], 12);
				HasAVX512VPOPCNTDQ	= bittest32(data[2], 14);
				HasRDPID			= bittest32(data[2], 22);
				HasCLDEMOTE			= bittest32(data[2], 25);
				HasMOVDIRI			= bittest32(data[2], 27);
				HasMOVDIR64B		= bittest32(data[2], 28);
				HasAVX5124VNNIW		= bittest32(data[3], 2);
				HasAVX5124FMAPS		= bittest32(data[3], 3);
				HasFSRM				= bittest32(data[3], 4);
				HasUINTR            = bittest32(data[3], 5);
				HasAVX512VP2I		= bittest32(data[3], 8);
				HasSERIALIZE		= bittest32(data[3], 14);
				hybrid_architecture = bittest32(data[3], 15);
				HasCET				= bittest32(data[3], 20);
				HasAMXBF16          = bittest32(data[3], 22);
				HasAVX512FP16       = bittest32(data[3], 23);
				HasAMXTILE          = bittest32(data[3], 24);
				HasAMXINT8          = bittest32(data[3], 25);
				switch (data[0]) {
					default: //case 1:
						__cpuidex(data, 7, 1);
						HasSHA512		= bittest32(data[0], 0);
						HasSM3			= bittest32(data[0], 1);
						HasSM4			= bittest32(data[0], 2);
						HasRAOINT		= bittest32(data[0], 3);
						HasAVXVNNI		= bittest32(data[0], 4);
						HasAVX512BF16	= bittest32(data[0], 5);
						HasCMPCCXADD	= bittest32(data[0], 7);
						HasFRMB0		= bittest32(data[0], 10);
						HasFRSB			= bittest32(data[0], 11);
						HasFRCSB		= bittest32(data[0], 12);
						HasAMXFP16		= bittest32(data[0], 21);
						HasAVXIFMA		= bittest32(data[0], 23);
						HasAVXVNNIINT8	= bittest32(data[3], 4);
						HasAVXNECONVERT	= bittest32(data[3], 5);
						HasAMXCOMPLEX	= bittest32(data[3], 8);
						HasAVXVNNIINT16 = bittest32(data[3], 10);
						HasPREFETCHI	= bittest32(data[3], 14);
						HasAVX10		= bittest32(data[3], 19);
						HasAPXF			= bittest32(data[3], 21);
					case 0:;
				}
			case 6: case 5: case 4: case 3: case 2:
				__cpuid(data, 0x20000000);
				if unexpected(data[0] > 0) {
					__cpuid(data, 0x20000001);
					HasMVEX = bittest32(data[3], 4);
				}
				__cpuid(data, 0x80000000);
				if ((uint32_t)data[0] > 0x80000000) {
					switch ((uint32_t)data[0]) {
						default: // case 0x80000021:
							__cpuid(data, 0x80000021);
							HasFRSB			|= bittest32(data[0], 10);
							HasFRCSB		|= bittest32(data[0], 11);
						case 0x80000020: case 0x8000001F: case 0x8000001E: case 0x8000001D: case 0x8000001C:
							__cpuid(data, 0x8000001C);
							HasLWPVAL		= bittest32(data[0], 1);
						case 0x8000001B: case 0x8000001A: case 0x80000019: case 0x80000018: case 0x80000017: case 0x80000016:
						case 0x80000015: case 0x80000014: case 0x80000013: case 0x80000012: case 0x80000011: case 0x80000010: case 0x8000000F:
						case 0x8000000E: case 0x8000000D: case 0x8000000C: case 0x8000000B: case 0x8000000A: case 0x80000009: case 0x80000008:
							__cpuid(data, 0x80000008);
							HasCLZERO		= bittest32(data[1], 0);
							xsave_restores_fpu_errors = bittest32(data[1], 2);
							HasRDPRU		= bittest32(data[1], 4);
							HasMCOMMIT		= bittest32(data[1], 8);
						case 0x80000007: case 0x80000006: case 0x80000005: case 0x80000004: case 0x80000003: case 0x80000002: case 0x80000001:
							__cpuid(data, 0x80000001);
							HasSYSCALL		= bittest32(data[3], 11);
							HasMMXEXT		= bittest32(data[3], 22);
							HasRDTSCP		= bittest32(data[3], 27);
							Has3DNOWEXT		= bittest32(data[3], 30);
							Has3DNOW		= bittest32(data[3], 31);
							HasLMLSAHF		= bittest32(data[2], 0);
							HasABM			= bittest32(data[2], 5);
							HasSSE4A		= bittest32(data[2], 6);
							HasMXCSRMM		= bittest32(data[2], 7);
							HasPREFETCHW	= bittest32(data[2], 8);
							HasXOP			= bittest32(data[2], 11);
							HasFMA4			= bittest32(data[2], 16);
							HasTBM			= bittest32(data[2], 21);
							HasMONITORX		= bittest32(data[2], 29);
					}
				}
			case 1:
				__cpuid(data, 1);
				HasTSC				= bittest32(data[3], 4);
				HasCMPXCHG8			= bittest32(data[3], 8);
				HasSYSENTER			= bittest32(data[3], 11);
				HasCMOV				= bittest32(data[3], 15);
				if (HasCLFLUSH		= bittest32(data[3], 19)) {
					cache_line_size = (data[1] & 0xFF00) >> 5;
				}
				HasMMX				= bittest32(data[3], 23);
				HasFXSAVE			= bittest32(data[3], 24);
				HasSSE				= bittest32(data[3], 25);
				HasSSE2				= bittest32(data[3], 26);
				HasSSE3				= bittest32(data[2], 0);
				HasPCLMULQDQ		= bittest32(data[2], 1);
				HasSSSE3			= bittest32(data[2], 9);
				HasFMA				= bittest32(data[2], 12);
				HasCMPXCHG16B		= bittest32(data[2], 13);
				HasSSE41			= bittest32(data[2], 19);
				HasSSE42			= bittest32(data[2], 20);
				HasMOVBE			= bittest32(data[2], 22);
				HasPOPCNT			= bittest32(data[2], 23);
				HasAES				= bittest32(data[2], 25);
				HasXSAVE			= bittest32(data[2], 26) & bittest32(data[2], 27);
				HasAVX				= bittest32(data[2], 28);
				HasF16C				= bittest32(data[2], 29);
				HasRDRAND			= bittest32(data[2], 30);

				// Fast system call/return compatibility table.
				// This isn't really *important* to anything that thcrap does,
				// but the shear inconsistency of it is laughable enough to document.
				//          | Legacy Modes                            | Long Modes
				//          | Real Mode | v8086 Mode | Protected Mode | Compatibility Mode | 64 Bit Mode
				// SYSENTER |           | Intel, AMD | Intel, AMD     | Intel              | Intel
				// SYSEXIT  |           |            | Intel, AMD     | Intel              | Intel
				// SYSCALL  | AMD(?)    | AMD        | AMD            | AMD                | Intel, AMD
				// SYSRET   |           |            | AMD            | AMD                | Intel, AMD
				//
				// Oh, and Intel sets the SYSCALL CPUID bit to 0 outside of 64 bit mode,
				// but AMD leaves the SYSENTER bit set in all modes. And that's why this
				// if statement is here.
				if (OSIsX64 && Manufacturer == AMD) {
					HasSYSENTER = false;
				}

				{
					union FamilyDataIn_t {
						uint32_t raw;
						struct {
							uint32_t stepping : 4;
							uint32_t model_id : 4;
							uint32_t family_id : 4;
							uint32_t processor_type : 2;
							uint32_t : 2;
							uint32_t extended_model_id : 4;
							uint32_t extended_family_id : 8;
							uint32_t : 4;
						};
					};
					FamilyDataIn_t family_data_in = { data0 };
					uint32_t temp = (uint32_t)-1;
					FamilyData_t family_data_out;
					family_data_out.stepping = family_data_in.stepping;
					family_data_out.model = family_data_in.model_id;
					family_data_out.family = family_data_in.family_id;
					switch (uint32_t temp = family_data_in.extended_family_id; family_data_in.family_id) {
						case 6:
							if (this->Manufacturer == Intel) {
								temp = 0;
						case 15:
								family_data_out.model |= family_data_in.extended_model_id << 4;
								family_data_out.family += temp;
							}
					}
					family_data_out.zero = 0;
					FamilyData = family_data_out;
				}
			case 0:;
		}
	}
};

static const CPUID_Data_t CPUID_Data;

bool CPU_Supports_SHA(void) {
	return CPUID_Data.HasSHA;
}

bool CPU_FDP_ErrorOnly(void) {
	return CPUID_Data.FDP_EXCPTN_ONLY;
}

bool CPU_FCS_FDS_Deprecated(void) {
	return CPUID_Data.FCS_FDS_DEP;
}

THCRAP_API bool OS_is_wine(void) {
	return wine_version != NULL;
}

#define WarnOnce(warning) do {\
	static bool AlreadyDisplayedWarning = false;\
	if (!AlreadyDisplayedWarning) { \
		warning; \
		AlreadyDisplayedWarning = true; \
	}\
} while (0)

static TH_NOINLINE void IncDecWarningMessage(void) {
	WarnOnce(log_print("EXPRESSION WARNING 0: Prefix increment and decrement operators do not currently function as expected because it is not possible to modify the value of an option in an expression. These operators only function to add one to a value, but do not actually modify it.\n"));
}

static TH_NOINLINE void AssignmentWarningMessage(void) {
	WarnOnce(log_print("EXPRESSION WARNING 1: Assignment operators do not currently function as expected because it is not possible to modify the value of an option in an expression. These operators are only included for future compatibility and operator precedence reasons.\n"));
}

static TH_NOINLINE void PatchValueWarningMessage(const char *const name) {
	log_printf("EXPRESSION WARNING 2: Unknown patch value type \"%s\", using value 0\n", name);
}

// The whole reason for having this is so that the log doesn't
// flood with warnings when calculating codecave sizes since
// their addresses haven't been found/recorded yet
static bool DisableCodecaveNotFound = false;
void DisableCodecaveNotFoundWarning(bool state) {
	DisableCodecaveNotFound = state;
}
static inline void CodecaveNotFoundWarningMessage(const char *const name, size_t name_length) {
	if (!DisableCodecaveNotFound) {
		log_printf("EXPRESSION WARNING 3: Codecave \"%.*s\" not found! Returning NULL...\n", name_length, name);
	}
}

static TH_NOINLINE void PostIncDecWarningMessage(void) {
	WarnOnce(log_print("EXPRESSION WARNING 4: Postfix increment and decrement operators do not currently function as expected because it is not possible to modify the value of an option in an expression. These operators do nothing and are only included for future compatibility and operator precedence reasons.\n"));
}

static inline void InvalidCPUFeatureWarningMessage(const char* name, size_t name_length) {
	log_printf("EXPRESSION WARNING 5: Unknown CPU feature \"%.*s\"! Assuming feature is not present and returning 0...\n", name_length, name);
}

static TH_NOINLINE void NullDerefWarningMessage(void) {
	log_print("EXPRESSION WARNING 6: Attempted to dereference NULL value! Returning NULL...\n");
}

static TH_NOINLINE void ExpressionErrorMessage(void) {
	log_print("EXPRESSION ERROR: Error parsing expression!\n");
}

static TH_NOINLINE void GroupingBracketErrorMessage(void) {
	log_print("EXPRESSION ERROR 0: Unmatched grouping brackets\n");
}

static TH_NOINLINE void ValueBracketErrorMessage(void) {
	log_print("EXPRESSION ERROR 1: Unmatched patch value brackets\n");
}

static TH_NOINLINE void BadCharacterErrorMessage(void) {
	log_print("EXPRESSION ERROR 2: Unknown character\n");
}

static inline void OptionNotFoundErrorMessage(const char* name, size_t name_length) {
	log_printf("EXPRESSION ERROR 3: Option \"%.*s\" not found\n", name_length, name);
}

static TH_NOINLINE void InvalidValueErrorMessage(const char *const str) {
	log_printf("EXPRESSION ERROR 4: Invalid value \"%s\"\n", str);
}

static TH_NOINLINE void InvalidPatchValueTypeErrorMessage(void) {
	log_print("EXPRESSION ERROR 5: Invalid patch value type!\n");
}

typedef uint8_t op_t;

typedef struct {
	const x86_reg_t *const regs;
	const uintptr_t rel_source;
	const HMODULE current_module;
} StackSaver;

static const char* eval_expr_impl(const char* expr, const char end, size_t *const out, const op_t start_op, const size_t start_value, const StackSaver *const data_refs);
static const char* consume_value_impl(const char* expr, size_t *const out, const StackSaver *const data_refs);

uint32_t* reg(x86_reg_t *regs, const char *regname, const char **endptr) {
	// Verify pointers and regname is at least 4 bytes
	if (!regs || !regname || !regname[0] || !regname[1] || !regname[2]) {
		return NULL;
	}
	// Just make sure that endptr can be written to unconditionally
	if (!endptr) endptr = (const char**)&endptr;

	*endptr = regname + 3;
	// Since regname is guaranteed to be at least 4
	// bytes long and none of `{|}~ are a valid register
	// name, the whole string can be converted to a
	// single case by clearing bit 5 of each character.
	switch (*(uint32_t*)regname & TextInt(0xDF, 0xDF, 0xDF, '\0')) {
		case TextInt('E', 'A', 'X'): return &regs->eax;
		case TextInt('E', 'C', 'X'): return &regs->ecx;
		case TextInt('E', 'D', 'X'): return &regs->edx;
		case TextInt('E', 'B', 'X'): return &regs->ebx;
		case TextInt('E', 'S', 'P'): return &regs->esp;
		case TextInt('E', 'B', 'P'): return &regs->ebp;
		case TextInt('E', 'S', 'I'): return &regs->esi;
		case TextInt('E', 'D', 'I'): return &regs->edi;
		default:
			*endptr = regname;
			return NULL;
	}
}

#define PreventOverlap(val) ((val) + (val))

#pragma warning(push)
// Intentional overflow/wraparound on some values,
// particularly variants of "or". Value only needs
// to be unique and easily calculated.
#pragma warning(disable : 4309 4369)
enum : uint8_t {
	NullOp = 0,
	StartNoOp = 1,
	EndGroupOp = 2,
	StandaloneTernaryEnd = ':',
	BadBrackets = 4,
	Power = '*' + '*',
	Multiply = '*',
	Divide = '/',
	Modulo = '%',
	Add = '+',
	Subtract = '-',
	ArithmeticLeftShift = '<' + '<',
	ArithmeticRightShift = '>' + '>',
	LogicalLeftShift = '<' + '<' + '<',
	LogicalRightShift = '>' + '>' + '>',
	CircularLeftShift = (PreventOverlap('r') + '<' + '<') % 256,
	CircularRightShift = (PreventOverlap('r') + '>' + '>') % 256,
	ThreeWay = '<' + '=' + '>',
	Less = '<',
	LessEqual = '<' + '=',
	Greater = '>',
	GreaterEqual = '>' + '=',
	Equal = '=' + '=',
	NotEqual = PreventOverlap('!') + '=',
	BitwiseAnd = PreventOverlap('&'),
	BitwiseNand = '~' + BitwiseAnd,
	BitwiseXor = '^',
	BitwiseXnor = '~' + BitwiseXor,
	BitwiseOr = PreventOverlap('|'),
	BitwiseNor = ('~' + BitwiseOr) % 256,
	LogicalAnd = PreventOverlap('&') + PreventOverlap('&'),
	LogicalAndSC = LogicalAnd - 1,
	LogicalNand = PreventOverlap('!') + LogicalAnd,
	LogicalNandSC = LogicalNand - 1,
	LogicalXor = '^' + '^',
	LogicalXnor = PreventOverlap('!') + LogicalXor,
	LogicalOr = (PreventOverlap('|') + PreventOverlap('|')) % 256,
	LogicalOrSC = LogicalOr - 1,
	LogicalNor = (PreventOverlap('!') + LogicalOr) % 256,
	LogicalNorSC = LogicalNor - 1,
	TernaryConditional = PreventOverlap('?'),
	Assign = '=',
	AddAssign = Add + '=',
	SubtractAssign = Subtract + '=',
	MultiplyAssign = Multiply + '=',
	DivideAssign = Divide + '=',
	ModuloAssign = Modulo + '=',
	ArithmeticLeftShiftAssign = ArithmeticLeftShift + '=',
	ArithmeticRightShiftAssign = ArithmeticRightShift + '=',
	LogicalLeftShiftAssign = LogicalLeftShift + '=',
	LogicalRightShiftAssign = LogicalRightShift + '=',
	CircularLeftShiftAssign = CircularLeftShift + '=',
	CircularRightShiftAssign = CircularRightShift + '=',
	AndAssign = BitwiseAnd + '=',
	NandAssign = (BitwiseNand + '=') % 256,
	XorAssign = BitwiseXor + '=',
	XnorAssign = (BitwiseXnor + '=') % 256,
	OrAssign = (BitwiseOr + '=') % 256,
	NorAssign = BitwiseNor + '=',
	Comma = ','
};
#pragma warning(pop)

enum : uint8_t {
	LeftAssociative = 0,
	RightAssociative = 1
};

struct OpData_t {
	uint8_t Precedence[256] = { 0 };
	uint8_t Associativity[256] = { 0 };
	constexpr OpData_t(void) {
#define ERROR_PRECEDENCE UINT8_MAX
#define ERROR_ASSOCIATIVITY LeftAssociative
		Precedence[BadBrackets] = ERROR_PRECEDENCE;
		Associativity[BadBrackets] = ERROR_ASSOCIATIVITY;
#define POWER_PRECEDENCE 18
#define POWER_ASSOCIATIVITY LeftAssociative
		Precedence[Power] = POWER_PRECEDENCE;
		Associativity[Power] = POWER_ASSOCIATIVITY;
#define MULTIPLY_PRECEDENCE 16
#define MULTIPLY_ASSOCIATIVITY LeftAssociative
		Precedence[Multiply] = Precedence[Divide] = Precedence[Modulo] = MULTIPLY_PRECEDENCE;
		Associativity[Multiply] = Associativity[Divide] = Associativity[Modulo] = MULTIPLY_ASSOCIATIVITY;
#define ADD_PRECEDENCE 15
#define ADD_ASSOCIATIVITY LeftAssociative
		Precedence[Add] = Precedence[Subtract] = ADD_PRECEDENCE;
		Associativity[Add] = Associativity[Subtract] = ADD_ASSOCIATIVITY;
#define SHIFT_PRECEDENCE 14
#define SHIFT_ASSOCIATIVITY LeftAssociative
		Precedence[LogicalLeftShift] = Precedence[LogicalRightShift] = Precedence[ArithmeticLeftShift] = Precedence[ArithmeticRightShift] = Precedence[CircularLeftShift] = Precedence[CircularRightShift] = SHIFT_PRECEDENCE;
		Associativity[LogicalLeftShift] = Associativity[LogicalRightShift] = Associativity[ArithmeticLeftShift] = Associativity[ArithmeticRightShift] = Associativity[CircularLeftShift] = Associativity[CircularRightShift] = SHIFT_ASSOCIATIVITY;
#define COMPARE_PRECEDENCE 13
#define COMPARE_ASSOCIATIVITY LeftAssociative
		Precedence[Less] = Precedence[LessEqual] = Precedence[Greater] = Precedence[GreaterEqual] = COMPARE_PRECEDENCE;
		Associativity[Less] = Associativity[LessEqual] = Associativity[Greater] = Associativity[GreaterEqual] = COMPARE_ASSOCIATIVITY;
#define EQUALITY_PRECEDENCE 12
#define EQUALITY_ASSOCIATIVITY LeftAssociative
		Precedence[Equal] = Precedence[NotEqual] = EQUALITY_PRECEDENCE;
		Associativity[Equal] = Associativity[NotEqual] = EQUALITY_ASSOCIATIVITY;
#define THREEWAY_PRECEDENCE 11
#define THREEWAY_ASSOCIATIVITY LeftAssociative
		Precedence[ThreeWay] = THREEWAY_PRECEDENCE;
		Associativity[ThreeWay] = THREEWAY_ASSOCIATIVITY;
#define BITAND_PRECEDENCE 10
#define BITAND_ASSOCIATIVITY LeftAssociative
		Precedence[BitwiseAnd] = Precedence[BitwiseNand] = BITAND_PRECEDENCE;
		Associativity[BitwiseAnd] = Associativity[BitwiseNand] = BITAND_ASSOCIATIVITY;
#define BITXOR_PRECEDENCE 9
#define BITXOR_ASSOCIATIVITY LeftAssociative
		Precedence[BitwiseXor] = Precedence[BitwiseXnor] = BITXOR_PRECEDENCE;
		Associativity[BitwiseXor] = Associativity[BitwiseXnor] = BITXOR_ASSOCIATIVITY;
#define BITOR_PRECEDENCE 8
#define BITOR_ASSOCIATIVITY LeftAssociative
		Precedence[BitwiseOr] = Precedence[BitwiseNor] = BITOR_PRECEDENCE;
		Associativity[BitwiseOr] = Associativity[BitwiseNor] = BITOR_ASSOCIATIVITY;
#define AND_PRECEDENCE 7
#define AND_ASSOCIATIVITY LeftAssociative
		Precedence[LogicalAnd] = Precedence[LogicalNand] = Precedence[LogicalAndSC] = Precedence[LogicalNandSC] = AND_PRECEDENCE;
		Associativity[LogicalAnd] = Associativity[LogicalNand] = Associativity[LogicalAndSC] = Associativity[LogicalNandSC] = AND_ASSOCIATIVITY;
#define XOR_PRECEDENCE 6
#define XOR_ASSOCIATIVITY LeftAssociative
		Precedence[LogicalXor] = Precedence[LogicalXnor] = XOR_PRECEDENCE;
		Associativity[LogicalXor] = Associativity[LogicalXnor] = XOR_ASSOCIATIVITY;
#define OR_PRECEDENCE 5
#define OR_ASSOCIATIVITY LeftAssociative
		Precedence[LogicalOr] = Precedence[LogicalNor] = Precedence[LogicalOrSC] = Precedence[LogicalNorSC] = OR_PRECEDENCE;
		Associativity[LogicalOr] = Associativity[LogicalNor] = Associativity[LogicalOrSC] = Associativity[LogicalNorSC] = OR_ASSOCIATIVITY;
#define TERNARY_PRECEDENCE 4
#define TERNARY_ASSOCIATIVITY RightAssociative
		Precedence[TernaryConditional] = TERNARY_PRECEDENCE;
		Associativity[TernaryConditional] = TERNARY_ASSOCIATIVITY;
#define ASSIGN_PRECEDENCE 3
#define ASSIGN_ASSOCIATIVITY RightAssociative
		Precedence[Assign] = Precedence[AddAssign] = Precedence[SubtractAssign] = Precedence[MultiplyAssign] = Precedence[DivideAssign] = Precedence[ModuloAssign] = Precedence[LogicalLeftShiftAssign] = Precedence[LogicalRightShiftAssign] = Precedence[ArithmeticLeftShiftAssign] = Precedence[ArithmeticRightShiftAssign] = Precedence[CircularLeftShiftAssign] = Precedence[CircularRightShiftAssign] = Precedence[AndAssign] = Precedence[OrAssign] = Precedence[XorAssign] = Precedence[NandAssign] = Precedence[XnorAssign] = Precedence[NorAssign] = ASSIGN_PRECEDENCE;
		Associativity[Assign] = Associativity[AddAssign] = Associativity[SubtractAssign] = Associativity[MultiplyAssign] = Associativity[DivideAssign] = Associativity[ModuloAssign] = Associativity[LogicalLeftShiftAssign] = Associativity[LogicalRightShiftAssign] = Associativity[ArithmeticLeftShiftAssign] = Associativity[ArithmeticRightShiftAssign] = Associativity[CircularLeftShiftAssign] = Associativity[CircularRightShiftAssign] = Associativity[AndAssign] = Associativity[OrAssign] = Associativity[XorAssign] = Associativity[NandAssign] = Associativity[XnorAssign] = Associativity[NorAssign] = ASSIGN_ASSOCIATIVITY;
#define COMMA_PRECEDENCE 2
#define COMMA_ASSOCIATIVITY LeftAssociative
		Precedence[Comma] = COMMA_PRECEDENCE;
		Associativity[Comma] = COMMA_ASSOCIATIVITY;
#define STARTOP_PRECEDENCE 1
#define STARTOP_ASSOCIATIVITY LeftAssociative
		Precedence[StartNoOp] = STARTOP_PRECEDENCE;
		Associativity[StartNoOp] = STARTOP_ASSOCIATIVITY;
#define NOOP_PRECEDENCE 0
#define NOOP_ASSOCIATIVITY LeftAssociative
		Precedence[NullOp] = Precedence[EndGroupOp] = Precedence[StandaloneTernaryEnd] = NOOP_PRECEDENCE;
		Associativity[NullOp] = Associativity[EndGroupOp] = Associativity[StandaloneTernaryEnd] = NOOP_ASSOCIATIVITY;
	}
};

static constexpr OpData_t OpData;

static inline const char* find_next_op_impl(const char* const expr, op_t* const out, char end) {
	uint8_t c;
	const char* expr_ref = expr - 1;
	while (1) {
		switch (c = *++expr_ref) {
			case '\0':
				*out = c;
				return expr;
			case '(': case '[': case '{':
				*out = BadBrackets;
				return expr;
			RetEndGroup: case ')': case ']': case '}': case ';':
				*out = EndGroupOp;
				return expr_ref;
			case '~':
				switch (uint8_t temp = expr_ref[1]) {
					case '&': case '|':
						temp += temp;
						[[fallthrough]];
					case '^':
						switch (expr_ref[2]) {
							case '=':
								*out = c + temp + '=';
								return expr_ref + 3;
							default:
								*out = c + temp;
								return expr_ref + 2;
						}
				}
				continue;
			case '!':
				c += c;
				if (uint8_t temp = expr_ref[1]; temp == '=') {
					goto CPlusEqualRetPlus2;
				}
				else {
					switch (temp) {
						case '&': case '|':
							temp += temp;
							[[fallthrough]];
						case '^':
							if (expr_ref[2] == expr_ref[1]) {
								*out = c + temp * 2;
								return expr_ref + 3;
							}
					}
				}
				continue;
			case '>': // Make sure the end of a patch value isn't confused with an operator
				if (end == '>') goto RetEndGroup;
			case '<': 
				if (expr_ref[1]) {
					uint32_t temp;
					if (expr_ref[2]) {
						switch (temp = *(uint32_t*)expr_ref & TextInt(0xFD, 0xFD, 0xFD, 0xFF)) {
							case TextInt('<', '<', '<', '='):
								*out = c * 3 + '=';
								return expr_ref + 4;
							default:
								switch (temp &= TextInt(0xFF, 0xFF, 0xFF, '\0')) {
									case TextInt('<', '=', '<'):
										*out = ThreeWay;
										return expr_ref + 3;
									case TextInt('<', '<', '='):
										goto CTimes2PlusEqualRetPlus3;
									case TextInt('<', '<', '<'):
										*out = c * 3;
										return expr_ref + 3;
								}
						}
						temp &= TextInt(0xFF, 0xFF, '\0');
					}
					else {
						temp = *(uint16_t*)expr_ref & TextInt(0xFD, 0xFD, '\0');
					}
					switch (temp) {
						case TextInt('<', '='):	goto CPlusEqualRetPlus2;
						case TextInt('<', '<'):	goto CTimes2RetPlus2;
					}
				}
				goto CRetPlus1;
			case 'r': case 'R':
				if (expr_ref[1] && expr_ref[2]) {
					c |= 0x20;
					c += c;
					switch (uint32_t temp = *(uint32_t*)expr_ref | TextInt(0x20, 0x00, 0x00, 0x00)) {
						case TextInt('r', '>', '>', '='):
							*out = c + '>' + '>' + '=';
							return expr_ref + 4;
						case TextInt('r', '<', '<', '='):
							*out = c + '<' + '<' + '=';
							return expr_ref + 4;
						default:
							switch (temp & TextInt(0xFF, 0xFF, 0xFF, '\0')) {
								case TextInt('r', '>', '>'):
									*out = c + '>' + '>';
									return expr_ref + 3;
								case TextInt('r', '<', '<'):
									*out = c + '<' + '<';
									return expr_ref + 3;
							}
					}
				}
				continue;
			case '&':
				c += c;
				switch (expr_ref[1]) {
					case '&':	goto CTimes2RetPlus2;
					case '=':	goto CPlusEqualRetPlus2;
					default:	goto CRetPlus1;
				}
			case '|':
				c += c;
				switch (expr_ref[1]) {
					case '|':	goto CTimes2RetPlus2;
					case '=':	goto CPlusEqualRetPlus2;
					default:	goto CRetPlus1;
				}
			case '*':
				switch (expr_ref[1]) {
					case '*':	goto CTimes2RetPlus2;
					case '=':	goto CPlusEqualRetPlus2;
					default:	goto CRetPlus1;
				}
			case '^':
				switch (expr_ref[1]) {
					case '^':	goto CTimes2RetPlus2;
					case '=':	goto CPlusEqualRetPlus2;
					default:	goto CRetPlus1;
				}
			case '+': case '-': case '/': case '%': case '=':
				switch (expr_ref[1]) {
					case '=':	goto CPlusEqualRetPlus2;
					default:	goto CRetPlus1;
				}
			case '?':
				c += c;
				[[fallthrough]];
			case ',':
				goto CRetPlus1;
			case ':':
				*out = c;
				return expr_ref;
		}
	}

CRetPlus1:
	*out = c;
	return expr_ref + 1;

CTimes2RetPlus2:
	*out = c * 2;
	return expr_ref + 2;

CPlusEqualRetPlus2:
	*out = c + '=';
	return expr_ref + 2;

CTimes2PlusEqualRetPlus3:
	*out = c * 2 + '=';
	return expr_ref + 3;
}

// Returns a string containing a textual representation of the operator
static TH_NOINLINE const char *const PrintOp(const op_t op) {
	switch (op) {
		case StartNoOp: return "StartNoOp";
		case Power: return "**";
		case Multiply: return "*";
		case Divide: return "/";
		case Modulo: return "%";
		case Add: return "+";
		case Subtract: return "-";
		case ArithmeticLeftShift: return "<<";
		case ArithmeticRightShift: return ">>";
		case LogicalLeftShift: return "<<<";
		case LogicalRightShift: return ">>>";
		case CircularLeftShift: return "R<<";
		case CircularRightShift: return "R>>";
		case BitwiseAnd: return "&";
		case BitwiseNand: return "~&";
		case BitwiseXor: return "^";
		case BitwiseXnor: return "~^";
		case BitwiseOr: return "|";
		case BitwiseNor: return "~|";
		case LogicalAnd: return "&&";
		case LogicalAndSC: return "&&SC";
		case LogicalNand: return "!&&";
		case LogicalNandSC: return "!&&SC";
		case LogicalXor: return "^^";
		case LogicalXnor: return "!^^";
		case LogicalOr: return "||";
		case LogicalOrSC: return "||SC";
		case LogicalNor: return "!||";
		case LogicalNorSC: return "!||SC";
		case Less: return "<";
		case LessEqual: return "<=";
		case Greater: return ">";
		case GreaterEqual: return ">=";
		case Equal: return "==";
		case NotEqual: return "!=";
		case ThreeWay: return "<=>";
		case TernaryConditional: return "?";
		case Assign: return "=";
		case AddAssign: return "+=";
		case SubtractAssign: return "-=";
		case MultiplyAssign: return "*=";
		case DivideAssign: return "/=";
		case ModuloAssign: return "%=";
		case ArithmeticLeftShiftAssign: return "<<=";
		case ArithmeticRightShiftAssign: return ">>=";
		case LogicalLeftShiftAssign: return "<<<=";
		case LogicalRightShiftAssign: return ">>>=";
		case CircularLeftShiftAssign: return "R<<=";
		case CircularRightShiftAssign: return "R>>=";
		case AndAssign: return "&=";
		case XorAssign: return "^=";
		case OrAssign: return "|=";
		case NandAssign: return "~&=";
		case XnorAssign: return "~^=";
		case NorAssign: return "~|=";
		case Comma: return ",";
		case NullOp: return "NullOp";
		case EndGroupOp: return "EndGroupNoOp";
		case StandaloneTernaryEnd: return "TernaryNoOp";
		case BadBrackets: return "BadBrackets";
		default: return "ERROR";
	}
}

enum : int8_t {
	HigherThanNext = 1,
	SameAsNext = 0,
	LowerThanNext = -1,
	LowerThanPrev = 1,
	SameAsPrev = 0,
	HigherThanPrev = -1
};

static inline size_t TH_FASTCALL ApplyPower(size_t value, size_t arg) {
	if (arg == 0) return 1;
	size_t result = 1;
	switch (unsigned long power; _BitScanReverse(&power, arg), power) {
#ifdef TH_X64
		case 5:
			result *= arg & 1 ? value : 1;
			arg >>= 1;
			value *= value;
#endif
		case 4:
			result *= arg & 1 ? value : 1;
			arg >>= 1;
			value *= value;
		case 3:
			result *= arg & 1 ? value : 1;
			arg >>= 1;
			value *= value;
		case 2:
			result *= arg & 1 ? value : 1;
			arg >>= 1;
			value *= value;
		case 1:
			result *= arg & 1 ? value : 1;
			arg >>= 1;
			value *= value;
		case 0:
			result *= arg & 1 ? value : 1;
			return result;
		default:
			return SIZE_MAX;
	}
}

static size_t ApplyOperator(const size_t value, const size_t arg, const op_t op) {
	switch (op) {
		case Power:
			return ApplyPower(value, arg);
		case MultiplyAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case Multiply:
			return value * arg;
		case DivideAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case Divide:
			return value / arg;
		case ModuloAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case Modulo:
			return value % arg;
		case AddAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case Add:
			return value + arg;
		case SubtractAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case Subtract:
			return value - arg;
		case LogicalLeftShiftAssign:
		case ArithmeticLeftShiftAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case LogicalLeftShift:
		case ArithmeticLeftShift:
			return value << arg;
		case LogicalRightShiftAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case LogicalRightShift:
			return value >> arg;
		case ArithmeticRightShiftAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case ArithmeticRightShift:
			return (size_t)((std::make_signed_t<size_t>)value >> arg);
		case CircularLeftShiftAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case CircularLeftShift: // Compiles to ROL
			return value << arg | value >> (sizeof(value) * CHAR_BIT - arg);
		case CircularRightShiftAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case CircularRightShift: // Compiles to ROR
			return value >> arg | value << (sizeof(value) * CHAR_BIT - arg);
		case Less:
			return value < arg;
		case LessEqual:
			return value <= arg;
		case Greater:
			return value > arg;
		case GreaterEqual:
			return value >= arg;
		case Equal:
			return value == arg;
		case NotEqual:
			return value != arg;
		case ThreeWay:
			return (value > arg) - (value < arg);
		case AndAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case BitwiseAnd:
			return value & arg;
		case NandAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case BitwiseNand:
			return ~(value & arg);
		case XorAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case BitwiseXor:
			return value ^ arg;
		case XnorAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case BitwiseXnor:
			return ~(value ^ arg);
		case OrAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case BitwiseOr:
			return value | arg;
		case NorAssign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case BitwiseNor:
			return ~(value | arg);
		case LogicalAnd: case LogicalAndSC:
			return value && arg;
		case LogicalNand: case LogicalNandSC:
			return !(value && arg);
		case LogicalXor:
			return (bool)(value ^ arg);
		case LogicalXnor:
			return (bool)!(value ^ arg);
		case LogicalOr: case LogicalOrSC:
			return value || arg;
		case LogicalNor: case LogicalNorSC:
			return !(value || arg);
		case Assign:
			AssignmentWarningMessage();
			[[fallthrough]];
		case Comma:
			//why tho
		case StartNoOp:
		case NullOp:
		case EndGroupOp:
		case StandaloneTernaryEnd:
		default:
			return arg;
	}
}

static inline const patch_val_t* GetOptionValue(const char* name, size_t name_length) {
	ExpressionLogging("Option: \"%.*s\"\n", name_length, name);
	const patch_val_t* const option = patch_opt_get_len(name, name_length);
	if unexpected(!option) {
		OptionNotFoundErrorMessage(name, name_length);
	}
	return option;
}

static inline const uint32_t GetPatchTestValue(const char* name, size_t name_length) {
	ExpressionLogging("PatchTest: \"%.*s\"\n", name_length, name);
	const patch_val_t* const patch_test = patch_opt_get_len(name, name_length);
	return patch_test ? patch_test->i : (uint32_t)patch_test; // Returns 0 if patch_test is NULL
}

static TH_NOINLINE uint32_t GetCPUFeatureTest(const char* name, size_t name_length) {
	ExpressionLogging("CPUFeatureTest: \"%.*s\"\n", name_length, name);
	// Yuck
	switch (name_length) {
		case 15:
			if		(strnicmp(name, "avx512vpopcntdq", name_length) == 0) return CPUID_Data.HasAVX512VPOPCNTDQ;
			else	goto InvalidCPUFeatureError;
			break;
		case 13:
			if		(strnicmp(name, "cachelinesize", name_length) == 0) return CPUID_Data.cache_line_size;
			else	goto InvalidCPUFeatureError;
			break;
		case 12:
			if		(strnicmp(name, "avx512bitalg", name_length) == 0) return CPUID_Data.HasAVX512BITALG;
			else if (strnicmp(name, "avx5124fmaps", name_length) == 0) return CPUID_Data.HasAVX5124FMAPS;
			else if (strnicmp(name, "avx5124vnniw", name_length) == 0) return CPUID_Data.HasAVX5124VNNIW;
			else if (strnicmp(name, "avxvnniint16", name_length) == 0) return CPUID_Data.HasAVXVNNIINT16;
			else if (strnicmp(name, "avxneconvert", name_length) == 0) return CPUID_Data.HasAVXNECONVERT;
			else	goto InvalidCPUFeatureError;
			break;
		case 11:
			if      (strnicmp(name, "xsavemasklo", name_length) == 0) return CPUID_Data.xsave_mask_low;
			else if (strnicmp(name, "xsavemaskhi", name_length) == 0) return CPUID_Data.xsave_mask_high;
			else if (strnicmp(name, "avx512vbmi1", name_length) == 0) return CPUID_Data.HasAVX512VBMI;
			else if (strnicmp(name, "avx512vbmi2", name_length) == 0) return CPUID_Data.HasAVX512VBMI2;
			else if (strnicmp(name, "avxvnniint8", name_length) == 0) return CPUID_Data.HasAVXVNNIINT8;
			else if (strnicmp(name, "prefetchwt1", name_length) == 0) return CPUID_Data.HasPREFETCHWT1;
			else	goto InvalidCPUFeatureError;
			break;
		case 10:
			if      (strnicmp(name, "xsavealign", name_length) == 0) return CPUID_Data.HasXSAVE ? 64 : 16;
			else if (strnicmp(name, "vpclmulqdq", name_length) == 0) return CPUID_Data.HasVPCLMULQDQ;
			else if	(strnicmp(name, "cmpxchg16b", name_length) == 0) return CPUID_Data.HasCMPXCHG16B;
			else if (strnicmp(name, "avx512ifma", name_length) == 0) return CPUID_Data.HasAVX512IFMA;
			else if (strnicmp(name, "avx512vbmi", name_length) == 0) return CPUID_Data.HasAVX512VBMI;
			else if (strnicmp(name, "avx512vp2i", name_length) == 0) return CPUID_Data.HasAVX512VP2I;
			else if (strnicmp(name, "avx512bf16", name_length) == 0) return CPUID_Data.HasAVX512BF16;
			else if (strnicmp(name, "clflushopt", name_length) == 0) return CPUID_Data.HasCLFLUSHOPT;
			else if (strnicmp(name, "avx512vnni", name_length) == 0) return CPUID_Data.HasAVX512VNNI;
			else if (strnicmp(name, "avx512fp16", name_length) == 0) return CPUID_Data.HasAVX512FP16;
			else if (strnicmp(name, "amxcomplex", name_length) == 0) return CPUID_Data.HasAMXCOMPLEX;
			else	goto InvalidCPUFeatureError;
			break;
		case 9:
			if      (strnicmp(name, "thcrapver", name_length) == 0) return PROJECT_VERSION;
			else if (strnicmp(name, "xsavesize", name_length) == 0) return CPUID_Data.xsave_max_size;
			else if (strnicmp(name, "pclmulqdq", name_length) == 0) return CPUID_Data.HasPCLMULQDQ;
			else if (strnicmp(name, "movdir64b", name_length) == 0) return CPUID_Data.HasMOVDIR64B;
			else if (strnicmp(name, "cmpccxadd", name_length) == 0) return CPUID_Data.HasCMPCCXADD;
			else if (strnicmp(name, "prefetchw", name_length) == 0) return CPUID_Data.HasPREFETCHW;
			else if (strnicmp(name, "prefetchi", name_length) == 0) return CPUID_Data.HasPREFETCHI;
			else if (strnicmp(name, "serialize", name_length) == 0) return CPUID_Data.HasSERIALIZE;
			else	goto InvalidCPUFeatureError;
			break;
		case 8:
			if		(strnicmp(name, "xsaveopt", name_length) == 0) return CPUID_Data.HasXSAVEOPT;
			else if	(strnicmp(name, "fsgsbase", name_length) == 0) return CPUID_Data.HasFSGSBASE;
			else if	(strnicmp(name, "cmpxchg8", name_length) == 0) return CPUID_Data.HasCMPXCHG8;
			else if (strnicmp(name, "avx512vl", name_length) == 0) return CPUID_Data.HasAVX512VL;
			else if (strnicmp(name, "avx512dq", name_length) == 0) return CPUID_Data.HasAVX512DQ;
			else if (strnicmp(name, "avx512bw", name_length) == 0) return CPUID_Data.HasAVX512BW;
			else if (strnicmp(name, "avx512cd", name_length) == 0) return CPUID_Data.HasAVX512CD;
			else if (strnicmp(name, "cldemote", name_length) == 0) return CPUID_Data.HasCLDEMOTE;
			else if (strnicmp(name, "sysenter", name_length) == 0) return CPUID_Data.HasSYSENTER;
			else if (strnicmp(name, "avx512er", name_length) == 0) return CPUID_Data.HasAVX512ER;
			else if (strnicmp(name, "avx512pf", name_length) == 0) return CPUID_Data.HasAVX512PF;
			else if (strnicmp(name, "3dnowext", name_length) == 0) return CPUID_Data.Has3DNOWEXT;
			else if (strnicmp(name, "monitorx", name_length) == 0) return CPUID_Data.HasMONITORX;
			else	goto InvalidCPUFeatureError;
			break;
		case 7:
			if		(strnicmp(name, "waitpkg", name_length) == 0) return CPUID_Data.HasWAITPKG;
			else if (strnicmp(name, "movdiri", name_length) == 0) return CPUID_Data.HasMOVDIRI;
			else if (strnicmp(name, "avxifma", name_length) == 0) return CPUID_Data.HasAVXIFMA;
			else if (strnicmp(name, "avx512f", name_length) == 0) return CPUID_Data.HasAVX512F;
			else if (strnicmp(name, "mxcsrmm", name_length) == 0) return CPUID_Data.HasMXCSRMM;
			else if (strnicmp(name, "lmlsahf", name_length) == 0) return CPUID_Data.HasLMLSAHF;
			else if (strnicmp(name, "clflush", name_length) == 0) return CPUID_Data.HasCLFLUSH;
			else if (strnicmp(name, "amxint8", name_length) == 0) return CPUID_Data.HasAMXINT8;
			else if (strnicmp(name, "amxtile", name_length) == 0) return CPUID_Data.HasAMXTILE;
			else if (strnicmp(name, "amxbf16", name_length) == 0) return CPUID_Data.HasAMXBF16;
			else if (strnicmp(name, "amxfp16", name_length) == 0) return CPUID_Data.HasAMXFP16;
			else if (strnicmp(name, "avxvnni", name_length) == 0) return CPUID_Data.HasAVXVNNI;
			else if (strnicmp(name, "mcommit", name_length) == 0) return CPUID_Data.HasMCOMMIT;
			else if (strnicmp(name, "syscall", name_length) == 0) return CPUID_Data.HasSYSCALL;
			else	goto InvalidCPUFeatureError;
			break;
		case 6:
			if      (strnicmp(name, "winver", name_length) == 0) return CPUID_Data.WindowsVersion.raw;
			else if (strnicmp(name, "popcnt", name_length) == 0) return CPUID_Data.HasPOPCNT;
			else if (strnicmp(name, "sha512", name_length) == 0) return CPUID_Data.HasSHA512;
			else if (strnicmp(name, "raoint", name_length) == 0) return CPUID_Data.HasRAOINT;
			else if (strnicmp(name, "rdtscp", name_length) == 0) return CPUID_Data.HasRDTSCP;
			else if (strnicmp(name, "xsavec", name_length) == 0) return CPUID_Data.HasXSAVEC;
			else if (strnicmp(name, "fxsave", name_length) == 0) return CPUID_Data.HasFXSAVE;
			else if (strnicmp(name, "mmxext", name_length) == 0) return CPUID_Data.HasMMXEXT;
			else if (strnicmp(name, "rdrand", name_length) == 0) return CPUID_Data.HasRDRAND;
			else if (strnicmp(name, "rdseed", name_length) == 0) return CPUID_Data.HasRDSEED;
			else if (strnicmp(name, "clzero", name_length) == 0) return CPUID_Data.HasCLZERO;
			else if (strnicmp(name, "lwpval", name_length) == 0) return CPUID_Data.HasLWPVAL;
			else if (strnicmp(name, "tsxhle", name_length) == 0) return CPUID_Data.HasTSXHLE;
			else if (strnicmp(name, "tsxrtm", name_length) == 0) return CPUID_Data.HasTSXRTM;
			else	goto InvalidCPUFeatureError;
			break;
		case 5:
			if      (strnicmp(name, "intel", name_length) == 0) return CPUID_Data.Manufacturer == Intel;
			else if (strnicmp(name, "ssse3", name_length) == 0) return CPUID_Data.HasSSSE3;
			else if (strnicmp(name, "sse41", name_length) == 0) return CPUID_Data.HasSSE41;
			else if (strnicmp(name, "sse42", name_length) == 0) return CPUID_Data.HasSSE42;
			else if (strnicmp(name, "sse4a", name_length) == 0) return CPUID_Data.HasSSE4A;
			else if (strnicmp(name, "movbe", name_length) == 0) return CPUID_Data.HasMOVBE;
			else if (strnicmp(name, "avx10", name_length) == 0) return CPUID_Data.HasAVX10;
			else if (strnicmp(name, "xsave", name_length) == 0) return CPUID_Data.HasXSAVE;
			else if (strnicmp(name, "shstk", name_length) == 0) return CPUID_Data.HasSHSTK;
			else if (strnicmp(name, "model", name_length) == 0) return CPUID_Data.FamilyData.raw;
			else if (strnicmp(name, "3dnow", name_length) == 0) return CPUID_Data.Has3DNOW;
			else if (strnicmp(name, "frmb0", name_length) == 0) return CPUID_Data.HasFRMB0;
			else if (strnicmp(name, "frcsb", name_length) == 0) return CPUID_Data.HasFRCSB;
			else if (strnicmp(name, "win64", name_length) == 0) return CPUID_Data.OSIsX64;
			else if (strnicmp(name, "rdpid", name_length) == 0) return CPUID_Data.HasRDPID;
			else if (strnicmp(name, "uintr", name_length) == 0) return CPUID_Data.HasUINTR;
			else if (strnicmp(name, "rdpru", name_length) == 0) return CPUID_Data.HasRDPRU;
			else	goto InvalidCPUFeatureError;
			break;
		case 4:
			if		(strnicmp(name, "sse3", name_length) == 0) return CPUID_Data.HasSSE3;
			else if (strnicmp(name, "avx2", name_length) == 0) return CPUID_Data.HasAVX2;
			else if (strnicmp(name, "bmi1", name_length) == 0) return CPUID_Data.HasBMI1;
			else if (strnicmp(name, "bmi2", name_length) == 0) return CPUID_Data.HasBMI2;
			else if (strnicmp(name, "fma4", name_length) == 0) return CPUID_Data.HasFMA4;
			else if (strnicmp(name, "vaes", name_length) == 0) return CPUID_Data.HasVAES;
			else if (strnicmp(name, "wine", name_length) == 0) return wine_version != NULL;
			else if (strnicmp(name, "apxf", name_length) == 0) return CPUID_Data.HasAPXF;
			else if	(strnicmp(name, "cmov", name_length) == 0) return CPUID_Data.HasCMOV;
			else if (strnicmp(name, "sse2", name_length) == 0) return CPUID_Data.HasSSE2;
			else if (strnicmp(name, "f16c", name_length) == 0) return CPUID_Data.HasF16C;
			else if (strnicmp(name, "gfni", name_length) == 0) return CPUID_Data.HasGFNI;
			else if (strnicmp(name, "erms", name_length) == 0) return CPUID_Data.HasERMS;
			else if (strnicmp(name, "fsrm", name_length) == 0) return CPUID_Data.HasFSRM;
			else if (strnicmp(name, "frsb", name_length) == 0) return CPUID_Data.HasFRSB;
			else if (strnicmp(name, "clwb", name_length) == 0) return CPUID_Data.HasCLWB;
			else if (strnicmp(name, "mvex", name_length) == 0) return CPUID_Data.HasMVEX;
			// A build compiled without SSE could theoretically run on PC98 hardware, so why not?
#if !TH_X86 || _M_IX86_FP == 0
			else if (strnicmp(name, "pc98", name_length) == 0) return USER_SHARED_DATA.AlternativeArchitecture == NEC98x86;
#endif
			else	goto InvalidCPUFeatureError;
			break;
		case 3:
			if		(strnicmp(name, "amd", name_length) == 0) return CPUID_Data.Manufacturer == AMD;
			else if (strnicmp(name, "avx", name_length) == 0) return CPUID_Data.HasAVX;
			else if (strnicmp(name, "fma", name_length) == 0) return CPUID_Data.HasFMA;
			else if (strnicmp(name, "bmi", name_length) == 0) return CPUID_Data.HasBMI1;
			else if (strnicmp(name, "adx", name_length) == 0) return CPUID_Data.HasADX;
			else if (strnicmp(name, "sha", name_length) == 0) return CPUID_Data.HasSHA;
			else if (strnicmp(name, "aes", name_length) == 0) return CPUID_Data.HasAES;
			else if (strnicmp(name, "abm", name_length) == 0) return CPUID_Data.HasABM;
			else if (strnicmp(name, "xop", name_length) == 0) return CPUID_Data.HasXOP;
			else if (strnicmp(name, "tbm", name_length) == 0) return CPUID_Data.HasTBM;
			else if (strnicmp(name, "sse", name_length) == 0) return CPUID_Data.HasSSE;
			else if (strnicmp(name, "mmx", name_length) == 0) return CPUID_Data.HasMMX;
			else if (strnicmp(name, "sm3", name_length) == 0) return CPUID_Data.HasSM3;
			else if (strnicmp(name, "sm4", name_length) == 0) return CPUID_Data.HasSM4;
			else if (strnicmp(name, "lwp", name_length) == 0) return CPUID_Data.HasLWP;
			else if (strnicmp(name, "cet", name_length) == 0) return CPUID_Data.HasCET;
			else if (strnicmp(name, "mpx", name_length) == 0) return CPUID_Data.HasMPX;
			else if (strnicmp(name, "pku", name_length) == 0) return CPUID_Data.HasPKU;
			else	goto InvalidCPUFeatureError;
			break;
		default:
InvalidCPUFeatureError:
			InvalidCPUFeatureWarningMessage(name, name_length);
			return false;
	}
}

static uintptr_t GetCodecaveAddress(const char *const name, const size_t name_length, const bool is_relative, const StackSaver *const data_refs) {
	ExpressionLogging("CodecaveAddress: \"%.*s\"\n", name_length, name);

	// memchr == strnchr
	const char* user_offset_expr = (const char*)memchr(name, '+', name_length);
	const size_t name_length_before_offset = user_offset_expr ? PtrDiffStrlen(user_offset_expr, name) : name_length;

	uintptr_t cave_addr = func_get_len(name, name_length_before_offset);
	if (!cave_addr) {
		CodecaveNotFoundWarningMessage(name, name_length_before_offset);
	}
	else {
		if (user_offset_expr) {
			++user_offset_expr;
			char* user_offset_expr_next;
			// Try a hex value by default for compatibility
			size_t user_offset_value = strtouz(user_offset_expr, &user_offset_expr_next, 16);
			switch ((bool)(user_offset_expr == user_offset_expr_next)) {
				case true: {
					// If a hex value doesn't work, try a subexpression
					if unexpected(!eval_expr_impl(user_offset_expr, is_relative ? ']' : '>', &user_offset_value, StartNoOp, 0, data_refs)) {
						ExpressionErrorMessage();
						break;
					}
					[[fallthrough]];
				}
				default:
					cave_addr += user_offset_value;
			}
		}
		if (is_relative) {
			cave_addr -= data_refs->rel_source + 4; // 
		}
	}
	return cave_addr;
}

static uintptr_t GetBPFuncOrRawAddress(const char *const name, const size_t name_length, bool is_relative, const StackSaver *const data_refs) {
	ExpressionLogging("BPFuncOrRawAddress: \"%.*s\"\n", name_length, name);
	uintptr_t addr = func_get_len(name, name_length);
	switch (addr) {
		case 0: {// Will be null if the name was not a BP function
			if unexpected(!eval_expr_impl(name, is_relative ? ']' : '>', &addr, StartNoOp, 0, data_refs)) {
				ExpressionErrorMessage();
				break;
			}
			[[fallthrough]];
		}
		default:
			if (is_relative) {
				addr -= data_refs->rel_source + 4;
			}
	}
	return addr;
}

static const char* NOP_Strings_Lookup[2][16] = {
	{/* Intel/Unknown NOP Strings */
		/*0x0*/ "",
		/*0x1*/ "90",
		/*0x2*/ "6690",
		/*0x3*/ "0F1F00",
		/*0x4*/ "0F1F4000",
		/*0x5*/ "0F1F440000",
		/*0x6*/ "660F1F440000",
		/*0x7*/ "0F1F8000000000",
		/*0x8*/ "0F1F840000000000",
		/*0x9*/ "660F1F840000000000",
		/*0xA*/ "662E0F1F840000000000",
		/*0xB*/ "66662E0F1F840000000000",
		/*0xC*/ "6666662E0F1F840000000000",
		/*0xD*/ "666666662E0F1F840000000000",
		/*0xE*/ "66666666662E0F1F840000000000",
		/*0xF*/ "6666666666662E0F1F840000000000"
	},
	{/* AMD NOP Strings */
		/*0x0*/ "",
		/*0x1*/ "90",
		/*0x2*/ "6690",
		/*0x3*/ "0F1F00",
		/*0x4*/ "0F1F4000",
		/*0x5*/ "0F1F440000",
		/*0x6*/ "660F1F440000",
		/*0x7*/ "0F1F8000000000",
		/*0x8*/ "0F1F840000000000",
		/*0x9*/ "660F1F840000000000",
		/*0xA*/ "662E0F1F840000000000",
		/*0xB*/ "0F1F440000660F1F440000",
		/*0xC*/ "660F1F440000660F1F440000",
		/*0xD*/ "660F1F4400000F1F8000000000",
		/*0xE*/ "0F1F80000000000F1F8000000000",
		/*0xF*/ "0F1F80000000000F1F840000000000"
	}
};

static patch_val_t GetMultibyteNOP(const char *const name, char end_char, const StackSaver *const data_refs) {
	patch_val_t nop_str;
	nop_str.type = PVT_CODE;
	nop_str.code.len = 0;
	(void)eval_expr_impl(name, end_char, &nop_str.code.len, StartNoOp, 0, data_refs);
	bool valid_nop_length = (nop_str.code.len != 0);
	nop_str.code.count = (size_t)valid_nop_length;
	if (valid_nop_length) {

		if (nop_str.code.len > 15) { // Max 15 bytes per instruction
			size_t count;
			size_t i = 16;
			while (--i) {
				count = nop_str.code.len / i;
				if (!(nop_str.code.len % i)) {
					break;
				}
			}
			nop_str.code.count = count;
			nop_str.code.len = i;
		}

		nop_str.code.ptr = NOP_Strings_Lookup[CPUID_Data.Manufacturer == AMD][nop_str.code.len];
	}
	return nop_str;
}

static patch_val_t GetMultibyteInt3(const char *const name, char end_char, const StackSaver *const data_refs) {
	patch_val_t int3_str;
	int3_str.type = PVT_CODE;
	int3_str.code.count = 0;
	(void)eval_expr_impl(name, end_char, &int3_str.code.count, StartNoOp, 0, data_refs);
	bool valid_int3_length = (int3_str.code.count != 0);
	int3_str.code.len = valid_int3_length;
	int3_str.code.ptr = "CC";
	return int3_str;
}

static inline const char* find_matching_end(const char* str, uint16_t delims_in) {
	union {
		uint16_t in;
		struct {
			char start;
			char end;
		};
	} delims = { delims_in };
	size_t depth = 0;
	--str;
	while (*++str) {
		depth += (*str == delims.start) - (*str == delims.end);
		if (!depth) return str;
	}
	return NULL;
};

static TH_NOINLINE const char* get_patch_value_impl(const char* expr, patch_val_t *const out, const StackSaver *const data_refs) {

	ExpressionLogging("Patch value opening char: \"%hhX\"\n", expr[0]);
	const char* patch_val_end = find_matching_end(expr, expr[0] == '[' ? TextInt('[', ']') : TextInt('<', '>'));
	ExpressionLogging("Patch value end: \"%s\"\n", patch_val_end ? patch_val_end : "NULL");
	if unexpected(!patch_val_end) {
		//Bracket error
		return NULL;
	}
	const bool is_relative = expr[0] == '[';
	// Increment expr so that the comparisons
	// don't check the opening bracket
	switch (*++expr) {
		case 'c':
			if (strnicmp(expr, "codecave:", 9) == 0) {
				out->type = is_relative ? PVT_DWORD : PVT_POINTER; // Relative offsets can only be 32 bits
				out->p = GetCodecaveAddress(expr, PtrDiffStrlen(patch_val_end, expr), is_relative, data_refs);
				return patch_val_end + 1;
			}
			else if (strnicmp(expr, "cpuid:", 6) == 0) {
				expr += 6;
				out->type = PVT_DWORD;
				out->i = GetCPUFeatureTest(expr, PtrDiffStrlen(patch_val_end, expr));
				return patch_val_end + 1;
			}
			break;
		case 'o':
			if (strnicmp(expr, "option:", 7) == 0) {
				expr += 7;
				out->type = PVT_UNKNOWN; // Will be overwritten if the option is valid
				if (const patch_val_t* option = GetOptionValue(expr, PtrDiffStrlen(patch_val_end, expr))) {
					*out = *option;
				}
				return patch_val_end + 1;
			}
			break;
		case 'p':
			if (strnicmp(expr, "patch:", 6) == 0) {
				out->type = PVT_DWORD;
				out->i = GetPatchTestValue(expr, PtrDiffStrlen(patch_val_end, expr));
				return patch_val_end + 1;
			}
			break;
		case 'n':
			if (strnicmp(expr, "nop:", 4) == 0) {
				expr += 4;
				*out = GetMultibyteNOP(expr, is_relative ? ']' : '>', data_refs);
				return patch_val_end + 1;
			}
			break;
		case 'i':
			if (strnicmp(expr, "int3:", 5) == 0) {
				expr += 5;
				*out = GetMultibyteInt3(expr, is_relative ? ']' : '>', data_refs);
				return patch_val_end + 1;
			}
			break;
	}
	out->type = PVT_POINTER;
	out->p = GetBPFuncOrRawAddress(expr, PtrDiffStrlen(patch_val_end, expr), is_relative, data_refs);
	return patch_val_end + 1;
};

const char* get_patch_value(const char* expr, patch_val_t* out, x86_reg_t* regs, uintptr_t rel_source, HMODULE hMod) {
	ExpressionLogging("START PATCH VALUE \"%s\"\n", expr);

	const StackSaver data_refs = { regs, rel_source, hMod };

	const char *const expr_next = get_patch_value_impl(expr, out, &data_refs);
	if (expr_next) {
		ExpressionLogging(
			"END PATCH VALUE\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tPatch value was: \"%s\"\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tFINAL result was: %zX / %zd / %zu\n"
			"Remaining after final: \"%s\"\n",
			expr, out->z, out->z, out->z, expr_next
		);
	}
	else {
		ExpressionErrorMessage();
		out->type = PVT_DEFAULT;
		out->z = 0;
		ExpressionLogging(
			"END PATCH VALUE WITH ERROR\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tPatch value was: \"%s\"\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tFINAL result was: %zX / %zd / %zu\n"
			"Remaining after final: \"%s\"\n",
			expr, out->z, out->z, out->z, expr
		);
	}
	return expr_next;
}

static inline const char* CheckCastType(const char* expr, uint8_t* out) {
	uint8_t type = 0;
	switch (*expr++ & 0xDF) {
		default:
			return NULL;
		case 'P':
			type = PVT_POINTER;
			break;
		case 'I':
			++type;
		case 'U':
			++type;
			switch (*expr++) {
				default:
					return NULL;
				case '6':
					if (*expr++ != '4') return NULL;
					type += 6;
					break;
				case '3':
					if (*expr++ != '2') return NULL;
					type += 4;
					break;
				case '1':
					if (*expr++ != '6') return NULL;
					type += 2;
					break;
				case '8':
					break;
			}
			break;
		case 'F':
			type = PVT_FLOAT;
			switch (*expr++) {
				default:
					return NULL;
				case '8':
					if (*expr++ != '0') return NULL;
					type += 2;
					break;
				case '6':
					if (*expr++ != '4') return NULL;
					type += 1;
					break;
				case '3':
					if (*expr++ != '2') return NULL;
			}
			break;
	}
	//expr += *expr == '*';
	if (*expr++ != ')') return NULL;
	*out = type;
	return expr;
}

static TH_FORCEINLINE const char* is_reg_name(const char* expr, const x86_reg_t *const regs, size_t* out) {

	enum : uint8_t {
#ifdef TH_X64
		// Yes, this results in different values
		// of REG_EDI/etc. on x64. This is intentional.
		REG_R15,
		REG_R14,
		REG_R13,
		REG_R12,
		REG_R11,
		REG_R10,
		REG_R9,
		REG_R8,
#endif
		
		REG_EDI, // 0b_000
		REG_ESI, // 0b_001
		REG_EBP, // 0b_010
		REG_ESP, // 0b_011
		REG_EBX, // 0b_100
		REG_EDX, // 0b_101
		REG_ECX, // 0b_110
		REG_EAX, // 0b_111
	};

	enum : uint8_t {
		REG_DW = 0b000, // 0
		REG_BL = 0b001, // 1
		REG_BH = 0b010, // 2
		REG_W  = 0b011, // 3
#ifdef TH_X64
		REG_QW = 0b100, // 4
#endif
	};

	bool deref;
	expr += !(deref = expr[0] != '&');

	uint8_t letter1 = *expr++ & 0xDF;
	uint8_t out_reg;
	uint8_t out_size;
#ifdef TH_X64
	if (letter1 == 'R') {
	    out_size = REG_QW;
		switch (letter1 = *expr++) {
			default:
				goto RexReg;
			case '1':
				letter1 = *expr++;
				if ((letter1 - '0') > 5) return NULL;
				out_reg = '5' - letter1;
				break;
			case '9':
				out_reg = REG_R9;
				break;
			case '8':
				out_reg = REG_R8;
		}
		switch (letter1 = *expr & 0xDF) {
			default:
				goto RegEnd;
			case 'B':
				out_size = REG_BL;
				break;
			case 'D':
				out_size = REG_DW;
				break;
			case 'W':
				out_size = REG_W;
				break;
		}
		++expr;
	}
	else {
#endif
		out_reg = REG_EDI;
		out_size = letter1 == 'E' ? letter1 = *expr++ & 0xDF, REG_DW : REG_W;
#ifdef TH_X64
	RexReg: // Only skipped on x86 to avoid an unused label warning
#endif
		uint8_t letter2 = *expr++ & 0xDF;
		switch (letter1) {
			default:
				return NULL;
			case 'S':
				out_reg |= REG_ESI;
				[[fallthrough]];
			case 'B':
				if (letter2 == 'P') {
					out_reg |= REG_EBP;
#ifdef TH_X64
					if (out_size != REG_W) goto RegEnd;
					bool low_reg;
					expr += (low_reg = ((*expr & 0xDF) == 'L'));
					out_size -= low_reg << 1;
#endif
					goto RegEnd;
				}
				[[fallthrough]];
			case 'D':
				if (letter2 == 'I') {
				    if (letter1 == 'B') return NULL;
#ifdef TH_X64
					if (out_size != REG_W) goto RegEnd;
					bool low_reg;
					expr += (low_reg = ((*expr & 0xDF) == 'L'));
					out_size -= low_reg << 1;
#endif
					goto RegEnd;
				}
				out_reg |= letter1 == 'B' ? REG_EBX : REG_EDX;
				break;
			case 'A':
				out_reg |= REG_EAX;
				break;
			case 'C':
				out_reg |= REG_ECX;
		}
		if (letter2 != 'X' && !(out_size &= ((letter2 == 'L') | ((letter2 == 'H') << 1)))) return NULL;
#ifdef TH_X64
	}
#endif
RegEnd:
	union single_reg {
#ifdef TH_X64
		uint64_t qword;
#endif
		uint32_t dword;
		uint16_t word;
		uint8_t byte;
	}* temp = (single_reg*)regs + out_reg + 1;
	(char*&)temp += (out_size == REG_BH);
	if (!deref) {
		*out = (uintptr_t)temp;
		return expr;
	}
	switch (out_size) {
#ifdef TH_X64
	    case REG_QW:
	        *out = temp->qword;
	        return expr;
#endif
		case REG_DW:
			*out = temp->dword;
			return expr;
		case REG_W:
			*out = temp->word;
			return expr;
		default:
			*out = temp->byte;
			return expr;
	}
}

static const char* consume_value_impl(const char* expr, size_t *const out, const StackSaver *const data_refs) {
	// cast is used for both casts and pointer sizes
	patch_val_t cur_value;
	cur_value.type = PVT_DEFAULT; // Default is register width

#define breakpoint_test_var data_refs->regs
#define is_breakpoint (breakpoint_test_var)
#define is_binhack (!breakpoint_test_var)

	const char* expr_next;

	--expr;
	while (1) {
		switch ((uint8_t)*++expr) {
			default:
				goto InvalidCharacterError;
			// Raw value or breakpoint register
			case '&':
			case 'a': case 'A': case 'c': case 'C': case 'e': case 'E':
			case 's': case 'S':
#ifdef TH_X64
			case 'r': case 'R':
#endif
			RawValueOrRegister:
				if (is_breakpoint) {
					if (expr_next = is_reg_name(expr, data_refs->regs, out)) {
						// current is set inside is_reg_name if a register is detected
						ExpressionLogging("Register value was %X / %d / %u\n", *out, *out, *out);
						goto PostfixCheck;
					}
				}
#ifdef TH_X86
			case 'r': case 'R': // Only relevant for Rx addresses on x86
#endif
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
				{
				RawValue:
					size_t current = str_to_addr(expr, expr_next, data_refs->current_module);
					if (expr == expr_next) {
						goto InvalidCharacterError;
					}
					ExpressionLogging(
						"Raw value was %X / %d / %u\n"
						"Remaining after raw value: \"%s\"\n",
						current, current, current, expr_next
					);
					*out = current;
					goto PostfixCheck;
				}
			// Current address
			case '@':
				*out = data_refs->rel_source;
				goto PostfixCheck;
			// Somehow it ran out of expression string, so stop parsing
			case '\0':
				goto InvalidValueError;
			// Skip whitespace
			case ' ': case '\t': case '\v': case '\f':
				continue;
			// Pointer sizes
			case 'b': case 'B':
				if (strnicmp(expr, "byte ptr", 8) == 0) {
					cur_value.type = PVT_BYTE;
					expr += 7;
					continue;
				}
				goto RawValueOrRegister;
			case 'w': case 'W':
				if (strnicmp(expr, "word ptr", 8) == 0) {
					cur_value.type = PVT_WORD;
					expr += 7;
					continue;
				}
				goto InvalidCharacterError;
			case 'd': case 'D':
				if (strnicmp(expr, "dword ptr", 9) == 0) {
					cur_value.type = PVT_DWORD;
					expr += 8;
					continue;
				}
				if (strnicmp(expr, "double ptr", 10) == 0) {
					cur_value.type = PVT_DOUBLE;
					expr += 9;
					continue;
				}
				goto RawValueOrRegister;
			case 'f': case 'F':
				if (strnicmp(expr, "float ptr", 9) == 0) {
					cur_value.type = PVT_FLOAT;
					expr += 8;
					continue;
				}
				goto RawValue;
			case 'q': case 'Q':
				if (strnicmp(expr, "qword ptr", 9) == 0) {
					cur_value.type = PVT_QWORD;
					expr += 8;
					continue;
				}
				goto InvalidCharacterError;
			case 't': case 'T':
				if (strnicmp(expr, "tbyte ptr", 9) == 0) {
					cur_value.type = PVT_LONGDOUBLE;
					expr += 8;
					continue;
				}
				goto InvalidCharacterError;
			// Unary Operators
			case '!': case '~': case '+': case '-': {
				expr_next = consume_value_impl(expr + 1 + (expr[0] == expr[1]), out, data_refs);
				if unexpected(!expr_next) goto InvalidValueError;
				switch ((uint8_t)expr[0] << (uint8_t)(expr[0] == expr[1])) {
					case '~': *out = ~*out; break;
					case '!': *out = !*out; break;
					case '-': *out *= -1; break;
					case '+': *out = +*out; break; // Optimized out
					case '~' << 1: *out = ~~*out; break; // Optimized out
					case '!' << 1: *out = (bool)*out; break;
					case '-' << 1: IncDecWarningMessage(); --*out; break;
					case '+' << 1: IncDecWarningMessage(); ++*out; break;
					default: TH_UNREACHABLE;
				}
				goto PostfixCheck;
			}
			case '*': {
				// expr + 1 is used to avoid creating a loop
				expr_next = consume_value_impl(expr + 1, out, data_refs);
				if unexpected(!expr_next) goto InvalidValueError;
				goto SharedDeref;
			}
			// Casts and subexpression values
			case '(': {
				// expr + 1 is used to avoid creating a loop
				++expr;
				expr_next = CheckCastType(expr, &cur_value.type);
				if (expr_next) {
					ExpressionLogging("Cast success\n");
					// Pointer casts only change the type
					// just like the "byte ptr" style casts
					/*
					if (expr_next[-2] == '*') {
						expr = expr_next;
						continue;
					}
					*/
					// Casts
					expr_next = consume_value_impl(expr_next, out, data_refs);
					if unexpected(!expr_next) goto InvalidValueError;
					++expr_next;
					if (cur_value.type != PVT_DEFAULT) {
						switch (cur_value.type) {
							case PVT_BYTE: *out = *(uint8_t*)out; break;
							case PVT_SBYTE: *out = *(int8_t*)out; break;
							case PVT_WORD: *out = *(uint16_t*)out; break;
							case PVT_SWORD: *out = *(int16_t*)out; break;
#ifdef TH_X64
							// DWORD is default on x86
							case PVT_DWORD: *out = *(uint32_t*)out; break;
#endif
							case PVT_SDWORD: *out = *(int32_t*)out; break;
#ifdef TH_X64
							case PVT_SQWORD: *out = *(int64_t*)out; break;
#endif
							case PVT_FLOAT: *out = (size_t)*(float*)out; break;
#ifdef TH_X64
							case PVT_DOUBLE: *out = (size_t)*(double*)out; break;
#endif
						}
					}
				}
				else {
					// Subexpressions
					expr_next = eval_expr_impl(expr, ')', out, StartNoOp, 0, data_refs);
					if unexpected(!expr_next) goto InvalidExpressionError;
					++expr_next;
				}
				goto PostfixCheck;
			}
			// Patch value and/or dereference
			case '[':
				if (is_breakpoint) {
					// Dereference
					// expr + 1 is used to avoid creating a loop
					expr_next = eval_expr_impl(expr + 1, ']', out, StartNoOp, 0, data_refs);
					if unexpected(!expr_next) goto InvalidExpressionError;
					++expr_next;
			SharedDeref:
					if unexpected(!*out) goto NullDerefWarning;
					if (cur_value.type == PVT_DEFAULT) {
						*out = *(size_t*)*out;
					}
					else {
						switch (cur_value.type) {
							case PVT_BYTE: *out = *(uint8_t*)*out; break;
							case PVT_SBYTE: *out = *(int8_t*)*out; break;
							case PVT_WORD: *out = *(uint16_t*)*out; break;
							case PVT_SWORD: *out = *(int16_t*)*out; break;
#ifdef TH_X64
							// DWORD is default on x86
							case PVT_DWORD: *out = *(uint32_t*)*out; break;
#endif
							case PVT_SDWORD: *out = *(int32_t*)*out; break;
#ifdef TH_X86
							// QWORD is default on x64
							case PVT_QWORD: *out = (size_t)*(uint64_t*)*out; break;
#endif
							case PVT_SQWORD: *out = (size_t)*(int64_t*)*out; break;
							case PVT_FLOAT: *out = (size_t)*(float*)*out; break;
							case PVT_DOUBLE: *out = (size_t)*(double*)*out; break;
							case PVT_LONGDOUBLE: *out = (size_t)*(LongDouble80*)*out; break;

							// This switch is only used when the type is set via
							// a cast operation, which doesn't include any of
							// the other patch value types.
							default: TH_UNREACHABLE;
						}
					}
					goto PostfixCheck;
				}
				[[fallthrough]];
			// Guaranteed patch value
			case '<': {
				// DON'T use expr + 1 since that kills get_patch_value
				expr_next = get_patch_value_impl(expr, &cur_value, data_refs);
				if unexpected(!expr_next) goto PatchValueBracketError;
				switch (cur_value.type) {
					case PVT_BYTE: *out = (size_t)cur_value.b; break;
					case PVT_SBYTE: *out = (size_t)cur_value.sb; break;
					case PVT_WORD: *out = (size_t)cur_value.w; break;
					case PVT_SWORD: *out = (size_t)cur_value.sw; break;
					case PVT_DWORD: *out = (size_t)cur_value.i; break;
					case PVT_SDWORD: *out = (size_t)cur_value.si; break;
					case PVT_QWORD: *out = (size_t)cur_value.q; break;
					case PVT_SQWORD: *out = (size_t)cur_value.sq; break;
					case PVT_FLOAT: *out = (size_t)cur_value.f; break;
					case PVT_DOUBLE: *out = (size_t)cur_value.d; break;
					case PVT_LONGDOUBLE: *out = (size_t)cur_value.ld; break;
					case PVT_STRING: *out = (size_t)cur_value.str.ptr; break;
					case PVT_STRING16: *out = (size_t)cur_value.str16.ptr; break;
					case PVT_STRING32: *out = (size_t)cur_value.str32.ptr; break;
					default: goto InvalidPatchValueTypeError;
				}
				ExpressionLogging("Parsed patch value is %X / %d / %u\n", *out, *out, *out);
				goto PostfixCheck;
			}
		}
	}

PostfixCheck:
	{
		if (uint8_t c = expr_next[0];
			(c == '+' || c == '-') && c == expr_next[1]) {
			PostIncDecWarningMessage();
			return expr_next + 2;
		}
		return expr_next;
	}

InvalidValueError:
	InvalidValueErrorMessage(expr);
	return NULL;
InvalidExpressionError:
	ExpressionErrorMessage();
	return NULL;
PatchValueBracketError:
	ValueBracketErrorMessage();
	return NULL;
InvalidCharacterError:
	BadCharacterErrorMessage();
	return NULL;
InvalidPatchValueTypeError:
	InvalidPatchValueTypeErrorMessage();
	return NULL;
NullDerefWarning:
	NullDerefWarningMessage();
	return expr;
}

static inline const char* TH_FASTCALL skip_value(const char* expr, const char end) {
	--expr;
	int depth = 0;
	while(1) {
		switch (char c = *++expr) {
			case '\0':
				if (end == '\0' && depth == 0) {
					return expr;
				}
				return NULL;
			default:
				if (c == end && depth == 0) {
					return expr;
				}
				continue;
			case '(': case '[':
				++depth;
				continue;
			case ')': case ']':
				if (c == end && depth == 0) {
					return expr;
				}
				else if (--depth < 0) {
					return NULL;
				}
				continue;
		}
	}
}

static size_t expr_index = 0;
static size_t dummy_cur_value = (size_t)&dummy_cur_value;

static const char* eval_expr_impl(const char* expr, char end, size_t *const out, const op_t start_op, const size_t start_value, const StackSaver *volatile data_refs) {

	ExpressionLogging(
		"START SUBEXPRESSION #%zu: \"%s\" with end \"%hhX\"\n"
		"Current value: %X / %d / %u\n",
		++expr_index, expr, end, start_value, start_value, start_value
	);

	size_t value = start_value;
	op_t ops_cur = start_op;
	op_t ops_next;
	size_t cur_value = 0;

	do {
		ExpressionLogging(
			"Remaining expression: \"%s\"\n"
			"Current character: %hhX\n",
			expr, expr[0]
		);

		if (ops_cur != NullOp) {
			const char* expr_next_val = consume_value_impl(expr, &cur_value, data_refs);
			if unexpected(!expr_next_val) goto InvalidValueError;
			expr = expr_next_val;
		}

		const char* expr_next_op = find_next_op_impl(expr, &ops_next, end);
		ExpressionLogging(
			"Found operator %hhX: \"%s\"\n"
			"Remaining after op: \"%s\"\n",
			ops_next, PrintOp(ops_next), expr_next_op
		);

		// Encountering an operator with a higher precedence can
		// be solved by recursing eval_expr over the remaining text
		// and treating the result as a single value
		switch (const uint8_t cur_prec = OpData.Precedence[ops_cur],
							  next_prec = OpData.Precedence[ops_next];
				(int8_t)((cur_prec > next_prec) - (cur_prec < next_prec))) {
			default:
				expr = expr_next_op;
				ExpressionLogging("\tLOWER PRECEDENCE than %s\n", PrintOp(ops_next));
				switch (ops_next) {
					default:
						expr = eval_expr_impl(expr, end, &cur_value, ops_next, cur_value, data_refs);
						if unexpected(!expr) goto InvalidExpressionError;
						ExpressionLogging(
							"\tRETURN FROM SUBEXPRESSION\n"
							"\tRemaining: \"%s\"\n",
							expr
						);
						if (expr[0] == '?') {
							++expr;

							[[fallthrough]];
					case TernaryConditional:
							if (cur_value) {
								ExpressionLogging("Ternary TRUE compare: \"%s\"\n", expr);
								if (expr[0] != ':') {
									expr = eval_expr_impl(expr, ':', &cur_value, StartNoOp, 0, data_refs);
									if unexpected(!expr) goto InvalidExpressionError;
								}
								ExpressionLogging("Skipping value until %hhX in \"%s\"...\n", end, expr);
								expr = skip_value(expr, end);
								if unexpected(!expr) goto InvalidExpressionError;
								ExpressionLogging(
									"Skipping completed\n"
									"Ternary TRUE remaining: \"%s\" with end \"%hhX\"\n",
									expr, end
								);
							}
							else {
								ExpressionLogging(
									"Ternary FALSE compare: \"%s\"\n"
									"Skipping value until %hhX in \"%s\"...\n",
									expr, ':', expr
								);
								DisableCodecaveNotFound = true;
								expr = eval_expr_impl(expr, ':', &dummy_cur_value, StartNoOp, 0, data_refs);
								DisableCodecaveNotFound = false;
								if unexpected(!expr) goto InvalidExpressionError;
								while (*expr++ != ':');
								ExpressionLogging(
									"Skipping completed\n"
									"Ternary FALSE remaining: \"%s\" with end \"%hhX\"\n",
									expr, end
								);
								continue;
							}
						}
						else if (expr[0] != end) {
							ops_next = NullOp;
						}
				}
				break;
			case SameAsNext:
				expr = expr_next_op;
				ExpressionLogging(
					"\tSAME PRECEDENCE with end \"%hhX\"\n",
					end
				);
				// This really should be looking at an actual associativity
				// property instead, but everything I tried resulted in
				// VS doubling the allocated stack space per call, which
				// is pretty terrible for a recursive function. Screw that.
				if ((uint8_t)(cur_prec - OpData.Precedence[Assign]) <= (OpData.Precedence[TernaryConditional] - OpData.Precedence[Assign])) {
					expr = eval_expr_impl(expr, end, &cur_value, ops_next, cur_value, data_refs);
					if unexpected(!expr) goto InvalidExpressionError;
				}
				/*switch (cur_prec) {
					case OpData.Precedence[TernaryConditional]:
					case OpData.Precedence[Assign]:
						expr = eval_expr_impl(expr, end, &cur_value, ops_next, cur_value, data_refs);
						if unexpected(!expr) goto InvalidExpressionError;
				}*/
				/*if (OpData.Associativity[ops_cur] == RightAssociative) {
					expr = eval_expr_impl(expr, end, &cur_value, ops_next, cur_value, data_refs);
					if unexpected(!expr) goto InvalidExpressionError;
				}*/
				break;
			case HigherThanNext:
				ExpressionLogging("\tHIGHER PRECEDENCE than %s\n", PrintOp(ops_next));
				// An op with a lower precedence can only be encountered when it's
				// necessary to end the current sub-expression, so apply the
				// current operator and then exit
				end = expr[0];
		}

		ExpressionLogging("Applying operation: \"%u %s %u\"\n", value, PrintOp(ops_cur), cur_value);
		value = ApplyOperator(value, cur_value, ops_cur);
		ExpressionLogging("Result of operator was %X / %d / %u\n", value, value, value);
		ops_cur = ops_next;

	} while (expr[0] != end);
	ExpressionLogging(
		"END SUBEXPRESSION #%zu: \"%s\"\n"
		"Subexpression value was %X / %d / %u\n",
		expr_index--, expr, value, value, value
	);
	*out = value;
	return expr;

InvalidValueError:
	InvalidValueErrorMessage(expr);
	return NULL;
InvalidExpressionError:
	ExpressionErrorMessage();
	return NULL;
}

const char* TH_FASTCALL eval_expr(const char* expr, char end, size_t* out, x86_reg_t* regs, uintptr_t rel_source, HMODULE hMod) {
	expr_index = 0;
	ExpressionLogging("START EXPRESSION \"%s\" with end \"%hhX\"\n", expr, end);

	const StackSaver data_refs = { regs, rel_source, hMod };

	const char *const expr_next = eval_expr_impl(expr, end, out, StartNoOp, 0, &data_refs);
	if (expr_next) {
		ExpressionLogging(
			"END EXPRESSION\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tExpression was: \"%s\"\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tFINAL result was: %X / %d / %u\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tRemaining after final: \"%s\"\n",
			expr, *out, *out, *out, expr_next
		);
	}
	else {
		ExpressionErrorMessage();
		*out = 0;
		ExpressionLogging(
			"END EXPRESSION WITH ERROR\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tExpression was: \"%s\"\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tFINAL result was: %X / %d / %u\n"
			"\t\t\t\t\t\t\t\t\t\t\t\t\tRemaining after final: \"%s\"\n",
			expr, *out, *out, *out, expr
		);
	}
	return expr_next;
}

void patch_val_set_op(const char* op_str, patch_val_t* Val) {
	if (op_str) {
		op_t op;
		(void)find_next_op_impl(op_str, &op, '\0');
		switch (op) {
			case MultiplyAssign:
			case DivideAssign:
			case ModuloAssign:
			case AddAssign:
			case SubtractAssign:
			case ArithmeticLeftShiftAssign:
			case LogicalLeftShiftAssign:
			case ArithmeticRightShiftAssign:
			case LogicalRightShiftAssign:
			case CircularLeftShiftAssign:
			case CircularRightShiftAssign:
			case AndAssign:
			case NandAssign:
			case XorAssign:
			case XnorAssign:
			case OrAssign:
			case NorAssign:
				op -= '=';
				[[fallthrough]];
			case Multiply:
			case Divide:
			case Modulo:
			case Add:
			case Subtract:
			case ArithmeticLeftShift:
			case LogicalLeftShift:
			case ArithmeticRightShift:
			case LogicalRightShift:
			case CircularLeftShift:
			case CircularRightShift:
			case BitwiseAnd:
			case BitwiseNand:
			case BitwiseXor:
			case BitwiseXnor:
			case BitwiseOr:
			case BitwiseNor:
				Val->merge_op = op;
				return;
		}
	}
	Val->merge_op = Add;
	return;
}

patch_val_t patch_val_not(patch_val_t Val) {
	// It'll be fiiiiiiine, right? (Probably not, but I'll fix it later)
	Val.q = ~Val.q;
	return Val;
}

#define PatchOpFloatVals(op) \
case PVT_FLOAT:			ret.f = Val1.f op Val2.f; break; \
case PVT_DOUBLE:		ret.d = Val1.d op Val2.d; break; \
case PVT_LONGDOUBLE:	ret.ld = Val1.ld op Val2.ld; break;

#define MakePatchOpFunc(op_name, op) \
patch_val_t patch_val_ ## op_name (patch_val_t Val1, patch_val_t Val2) { \
	patch_val_t ret; \
	switch (ret.type = Val1.type) { \
		case PVT_BYTE:			ret.b = Val1.b op Val2.b; break; \
		case PVT_SBYTE:			ret.sb = Val1.sb op Val2.sb; break; \
		case PVT_WORD:			ret.w = Val1.w op Val2.w; break; \
		case PVT_SWORD:			ret.sw = Val1.sw op Val2.sw; break; \
		case PVT_DWORD:			ret.i = Val1.i op Val2.i; break; \
		case PVT_SDWORD:		ret.si = Val1.si op Val2.si; break; \
		case PVT_QWORD:			ret.q = Val1.q op Val2.q; break; \
		case PVT_SQWORD:		ret.sq = Val1.sq op Val2.sq; break; \
		PatchOpFloatVals(op) \
		default: \
			ret.type = PVT_NONE; \
	} \
	return ret; \
}

MakePatchOpFunc(add, +);
MakePatchOpFunc(sub, -);
MakePatchOpFunc(mul, *);
MakePatchOpFunc(div, /);

#undef PatchOpFloatVals
#define PatchOpFloatVals(op)

MakePatchOpFunc(mod, %);
MakePatchOpFunc(shl, <<);
MakePatchOpFunc(shr, >>);
MakePatchOpFunc(and, &);
MakePatchOpFunc(or, |);
MakePatchOpFunc(xor, ^);

patch_val_t patch_val_rol(patch_val_t Val1, patch_val_t Val2) {
	patch_val_t ret;
	switch (ret.type = Val1.type) {
		case PVT_BYTE:			ret.b = Val1.b << Val2.b | Val1.b >> (sizeof(Val1.b) * CHAR_BIT - Val2.b); break;
		case PVT_SBYTE:			ret.sb = Val1.sb << Val2.sb | Val1.sb >> (sizeof(Val1.sb) * CHAR_BIT - Val2.sb); break;
		case PVT_WORD:			ret.w = Val1.w << Val2.w | Val1.w >> (sizeof(Val1.w) * CHAR_BIT - Val2.w); break;
		case PVT_SWORD:			ret.sw = Val1.sw << Val2.sw | Val1.sw >> (sizeof(Val1.sw) * CHAR_BIT - Val2.sw); break;
		case PVT_DWORD:			ret.i = Val1.i << Val2.i | Val1.i >> (sizeof(Val1.i) * CHAR_BIT - Val2.i); break;
		case PVT_SDWORD:		ret.si = Val1.si << Val2.si | Val1.si >> (sizeof(Val1.si) * CHAR_BIT - Val2.si); break;
		case PVT_QWORD:			ret.q = Val1.q << Val2.q | Val1.q >> (sizeof(Val1.q) * CHAR_BIT - Val2.q); break;
		case PVT_SQWORD:		ret.sq = Val1.sq << Val2.sq | Val1.sq >> (sizeof(Val1.sq) * CHAR_BIT - Val2.sq); break;
		PatchOpFloatVals(op)
		default:
			ret.type = PVT_NONE;
	}
	return ret;
}

patch_val_t patch_val_ror(patch_val_t Val1, patch_val_t Val2) {
	patch_val_t ret;
	switch (ret.type = Val1.type) {
		case PVT_BYTE:			ret.b = Val1.b >> Val2.b | Val1.b << (sizeof(Val1.b) * CHAR_BIT - Val2.b); break;
		case PVT_SBYTE:			ret.sb = Val1.sb >> Val2.sb | Val1.sb << (sizeof(Val1.sb) * CHAR_BIT - Val2.sb); break;
		case PVT_WORD:			ret.w = Val1.w >> Val2.w | Val1.w << (sizeof(Val1.w) * CHAR_BIT - Val2.w); break;
		case PVT_SWORD:			ret.sw = Val1.sw >> Val2.sw | Val1.sw << (sizeof(Val1.sw) * CHAR_BIT - Val2.sw); break;
		case PVT_DWORD:			ret.i = Val1.i >> Val2.i | Val1.i << (sizeof(Val1.i) * CHAR_BIT - Val2.i); break;
		case PVT_SDWORD:		ret.si = Val1.si >> Val2.si | Val1.si << (sizeof(Val1.si) * CHAR_BIT - Val2.si); break;
		case PVT_QWORD:			ret.q = Val1.q >> Val2.q | Val1.q << (sizeof(Val1.q) * CHAR_BIT - Val2.q); break;
		case PVT_SQWORD:		ret.sq = Val1.sq >> Val2.sq | Val1.sq << (sizeof(Val1.sq) * CHAR_BIT - Val2.sq); break;
			PatchOpFloatVals(op)
		default:
			ret.type = PVT_NONE;
	}
	return ret;
}

patch_val_t patch_val_op_str(const char* op_str, patch_val_t Val1, patch_val_t Val2) {
	op_t op;
	if (op_str) {
		(void)find_next_op_impl(op_str, &op, '\0');
	}
	else {
		op = Val1.merge_op;
	}
	switch (op) {
		case MultiplyAssign: case Multiply:
			return patch_val_mul(Val1, Val2);
		case DivideAssign: case Divide:
			return patch_val_div(Val1, Val2);
		case ModuloAssign: case Modulo:
			return patch_val_mod(Val1, Val2);
		case AddAssign: case Add:
			return patch_val_add(Val1, Val2);
		case SubtractAssign: case Subtract:
			return patch_val_sub(Val1, Val2);
		case ArithmeticLeftShiftAssign: case ArithmeticLeftShift:
		case LogicalLeftShiftAssign: case LogicalLeftShift:
			return patch_val_shr(Val1, Val2);
		case ArithmeticRightShiftAssign: case ArithmeticRightShift:
		case LogicalRightShiftAssign: case LogicalRightShift:
			return patch_val_shl(Val1, Val2);
		case CircularLeftShiftAssign: case CircularLeftShift:
			return patch_val_rol(Val1, Val2);
		case CircularRightShiftAssign: case CircularRightShift:
			return patch_val_ror(Val1, Val2);
		case AndAssign: case BitwiseAnd:
			return patch_val_and(Val1, Val2);
		case NandAssign: case BitwiseNand:
			return patch_val_not(patch_val_and(Val1, Val2));
		case XorAssign: case BitwiseXor:
			return patch_val_xor(Val1, Val2);
		case XnorAssign: case BitwiseXnor:
			return patch_val_not(patch_val_xor(Val1, Val2));
		case OrAssign: case BitwiseOr:
			return patch_val_or(Val1, Val2);
		case NorAssign: case BitwiseNor:
			return patch_val_not(patch_val_or(Val1, Val2));
		case ThreeWay: case Less: case LessEqual: case Greater: case GreaterEqual: case Equal: case NotEqual: case LogicalAnd: case LogicalNand: case LogicalXor: case LogicalXnor: case LogicalOr: case LogicalNor:
			log_print("Options cannot use logical or comparison operators!\n");
			break;
		case Assign:
			log_print("Options cannot use assignment!\n");
			break;
		case Comma:
			log_print("but why tho\n");
			break;
	}
	{
		patch_val_t bad_ret;
		bad_ret.type = PVT_NONE;
		return bad_ret;
	}
}
