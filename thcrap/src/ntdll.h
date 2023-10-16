/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * ntdll shim functions and
  * internal windows structs
  */

typedef LONG NTSTATUS;
typedef LONG_PTR KPRIORITY;

#define STATUS_SUCCESS ((NTSTATUS)0L)
#define STATUS_INVALID_INFO_CLASS ((NTSTATUS)0xC0000003L)

struct PROCESS_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    PEB* PebBaseAddress;
    ULONG_PTR AffinityMask;
    KPRIORITY BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
};

// https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntqueryinformationprocess
typedef enum {
    ProcessBasicInformation = 0,
    ProcessWow64Information = 26,
    ProcessImageFileName = 27
} PROCESSINFOCLASS;

// NTSTATUS NTAPI NtQueryInformationProcess(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength);
func_ptr_typedef(NTSTATUS, NTAPI, NtQueryInformationProcessPtr)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
extern NtQueryInformationProcessPtr NtQueryInformationProcess;

#if __cplusplus
extern "C++" static TH_FORCEINLINE bool NtGetProcessBasicInfomation(HANDLE ProcessHandle, PROCESS_BASIC_INFORMATION& pbi) {
    return STATUS_SUCCESS == NtQueryInformationProcess(ProcessHandle, ProcessBasicInformation, &pbi, sizeof(PROCESS_BASIC_INFORMATION), NULL);
}
#endif

static TH_FORCEINLINE bool NtIsWow64Process(HANDLE ProcessHandle) {
    ULONG_PTR is_wow64;
    return STATUS_SUCCESS == NtQueryInformationProcess(ProcessHandle, ProcessWow64Information, &is_wow64, sizeof(ULONG_PTR), NULL) && is_wow64;
}

#if TH_X64
// NTSTATUS NTAPI NtAllocateVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect);
func_ptr_typedef(NTSTATUS, NTAPI, NtAllocateVirtualMemoryPtr)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
// NTSTATUS NTAPI NtFreeVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize, ULONG FreeType);
func_ptr_typedef(NTSTATUS, NTAPI, NtFreeVirtualMemoryPtr)(HANDLE, PVOID*, PSIZE_T, ULONG);
// ULONG NTAPI RtlNtStatusToDosError(NTSTATUS Status);
func_ptr_typedef(ULONG, NTAPI, RtlNtStatusToDosErrorPtr)(NTSTATUS);

extern NtAllocateVirtualMemoryPtr NtAllocateVirtualMemory;
extern NtFreeVirtualMemoryPtr NtFreeVirtualMemory;
extern RtlNtStatusToDosErrorPtr RtlNtStatusToDosError;
#endif


/*
Struct definitions based on the fields documented to have
consistent offsets in all Windows versions 5.0+
TEB: https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/api/pebteb/teb/index.htm
PEB: https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/api/pebteb/peb/index.htm
*/

typedef struct _PEB PEB;
struct _PEB {
	BOOLEAN InheritedAddressSpace;
	BOOLEAN ReadImageFileExecOptions;
	BOOLEAN BeingDebugged;
	BOOLEAN SpareBool;
#ifdef TH_X64
	UCHAR Padding0[4];
#endif
	HANDLE Mutant;
	PVOID ImageBaseAddress;
	PVOID Ldr; // PEB_LDR_DATA*
	PVOID ProcessParameters; // RTL_USER_PROCESS_PARAMETERS*
	PVOID SubSystemData;
	HANDLE ProcessHeap;
	RTL_CRITICAL_SECTION* FastPebLock;
	PVOID unreliable_member_1;
	PVOID unreliable_member_2;
	ULONG unreliable_member_3;
#ifdef TH_X64
	UCHAR Padding1[4];
#endif
	PVOID KernelCallbackTable;
	ULONG SystemReserved[2];
	PVOID unreliable_member_4;
	ULONG TlsExpansionCounter;
#ifdef TH_X64
	UCHAR Padding2[4];
#endif
	PVOID TlsBitmap;
	ULONG TlsBitmapBits[2];
	PVOID ReadOnlySharedMemoryBase;
	PVOID unreliable_member_5;
	PVOID* ReadOnlyStaticServerData;
	PVOID AnsiCodePageData;
	PVOID OemCodePageData;
	PVOID UnicodeCaseTableData;
	ULONG NumberOfProcessors;
	ULONG NtGlobalFlag;
	LARGE_INTEGER CriticalSectionTimeout;
	ULONG_PTR HeapSegmentReserve;
	ULONG_PTR HeapSegmentCommit;
	ULONG_PTR HeapDeCommitTotalFreeThreshold;
	ULONG_PTR HeapDeCommitFreeBlockThreshold;
	ULONG NumberOfHeaps;
	ULONG MaximumNumberOfHeaps;
	PVOID* ProcessHeaps;
	PVOID GdiSharedHandleTable;
	PVOID ProcessStarterHelper;
	ULONG GdiDCAttributeList;
#ifdef TH_X64
	UCHAR Padding3[4];
#endif
	RTL_CRITICAL_SECTION* LoaderLock;
	ULONG OSMajorVersion;
	ULONG OSMinorVersion;
	USHORT OSBuildNumber;
	union {
		USHORT OSCSDVersion;
		struct {
			BYTE OSCSDMajorVersion;
			BYTE OSCSDMinorVersion;
		};
	};
	ULONG OSPlatformId;
	ULONG ImageSubsystem;
	ULONG ImageSubsystemMajorVersion;
	ULONG ImageSubsystemMinorVersion;
#ifdef TH_X64
	UCHAR Padding4[4];
#endif
	KAFFINITY unreliable_member_6;
#ifdef TH_X64
	ULONG GdiHandleBuffer[0x3C];
#else
	ULONG GdiHandleBuffer[0x22];
#endif
	VOID(*PostProcessInitRoutine)(VOID);
	PVOID TlsExpansionBitmap;
	ULONG TlsExpansionBitmapBits[0x20];
	ULONG SessionId;
#ifdef TH_X64
	UCHAR Padding5[4];
#endif
};
typedef struct _CLIENT_ID {
	HANDLE UniqueProcess;
	HANDLE UniqueThread;
} CLIENT_ID;
typedef struct _GDI_TEB_BATCH {
	ULONG Offset;
	ULONG_PTR HDC;
	ULONG Buffer[310];
} GDI_TEB_BATCH, *PGDI_TEB_BATCH;
typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
typedef struct _TEB TEB;
struct _TEB {
	//NT_TIB NtTib;
	struct _EXCEPTION_REGISTRATION_RECORD* ExceptionList;
	PVOID StackBase;
	PVOID StackLimit;
	PVOID SubSystemTib;
	PVOID FiberData;
	PVOID ArbitraryUserPointer;
	TEB* Self;
	PVOID EnvironmentPointer;
	CLIENT_ID ClientId;
	PVOID ActiveRpcHandle;
	PVOID ThreadLocalStoragePointer;
	PEB* ProcessEnvironmentBlock;
	ULONG LastErrorValue;
	ULONG CountOfOwnedCriticalSections;
	PVOID CsrClientThread;
	PVOID Win32ThreadInfo;
	ULONG User32Reserved[0x1A];
	ULONG UserReserved[5];
	PVOID WOW32Reserved;
	ULONG CurrentLocale;
	ULONG FpSoftwareStatusRegister;
	PVOID SystemReserved1[0x36];
	LONG ExceptionCode;
#ifdef TH_X64
	UCHAR Padding0[4];
#endif
	UCHAR SpareBytes1[0x2C];
	GDI_TEB_BATCH GdiTebBatch;
	CLIENT_ID RealClientId;
	PVOID GdiCachedProcessHandle;
	ULONG GdiClientPID;
	ULONG GdiClientTID;
	PVOID GdiThreadLocalInfo;
	ULONG_PTR Win32ClientInfo[0x3E];
	PVOID glDispatchTable[0xE9];
	ULONG_PTR glReserved1[0x1D];
	PVOID glReserved2;
	PVOID glSectionInfo;
	PVOID glSection;
	PVOID glTable;
	PVOID glCurrentRC;
	PVOID glContext;
	ULONG LastStatusValue;
#ifdef TH_X64
	UCHAR Padding2[4];
#endif
	UNICODE_STRING StaticUnicodeString;
	union {
		WCHAR StaticUnicodeBuffer[MAX_PATH + 1];
		char StaticUTF8Buffer[(MAX_PATH + 1) * sizeof(WCHAR)];
	};
#ifdef TH_X64
	UCHAR Padding3[6];
#endif
	PVOID DeallocationStack;
	PVOID TlsSlots[0x40];
	LIST_ENTRY TlsLinks;
	PVOID Vdm;
	PVOID ReservedForNtRpc;
	HANDLE DbgSsReserved[2];
};

#ifdef TH_X64
#define read_teb_member(member) (\
member_size(TEB, member) == 1 ? read_gs_byte(offsetof(TEB, member)) : \
member_size(TEB, member) == 2 ? read_gs_word(offsetof(TEB, member)) : \
member_size(TEB, member) == 4 ? read_gs_dword(offsetof(TEB, member)) : \
read_gs_qword(offsetof(TEB, member)) \
)
#define write_teb_member(member, data) (\
member_size(TEB, member) == 1 ? write_gs_byte(offsetof(TEB, member), (data)) : \
member_size(TEB, member) == 2 ? write_gs_word(offsetof(TEB, member), (data)) : \
member_size(TEB, member) == 4 ? write_gs_dword(offsetof(TEB, member), (data)) : \
write_gs_qword(offsetof(TEB, member), (data)) \
)
#else
#define read_teb_member(member) (\
member_size(TEB, member) == 1 ? read_fs_byte(offsetof(TEB, member)) : \
member_size(TEB, member) == 2 ? read_fs_word(offsetof(TEB, member)) : \
read_fs_dword(offsetof(TEB, member)) \
)
#define write_teb_member(member, data) (\
member_size(TEB, member) == 1 ? write_fs_byte(offsetof(TEB, member), (data)) : \
member_size(TEB, member) == 2 ? write_fs_word(offsetof(TEB, member), (data)) : \
write_fs_dword(offsetof(TEB, member), (data)) \
)
#endif
#define CurrentTeb() ((TEB*)read_teb_member(Self))
#define CurrentPeb() ((PEB*)read_teb_member(ProcessEnvironmentBlock))

struct KSYSTEM_TIME {
	ULONG LowPart;
	LONG High1Time;
	LONG High2Time;
};

enum NT_PRODUCT_TYPE {
	NtProductWinNt = 1,
	NtProductLanManNt = 2,
	NtProductServer = 3
};

enum ALTERNATIVE_ARCHITECTURE_TYPE {
	StandardDesign = 0,
	NEC98x86 = 1,
	EndAlternatives = 2
};

#define NX_SUPPORT_POLICY_ALWAYSOFF (0)
#define NX_SUPPORT_POLICY_ALWAYSON (1)
#define NX_SUPPORT_POLICY_OPTIN (2)
#define NX_SUPPORT_POLICY_OPTOUT (3)

#define SEH_VALIDATION_POLICY_ON (0)
#define SEH_VALIDATION_POLICY_OFF (1)
#define SEH_VALIDATION_POLICY_TELEMETRY (2)
#define SEH_VALIDATION_POLICY_DEFER (3)

/*
Struct definitions based on the fields documented to have
consistent offsets in any windows version, with notes
indicating which versions added particular fields
https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/api/ntexapi_x/kuser_shared_data/index.htm
*/

typedef struct _KUSER_SHARED_DATA KUSER_SHARED_DATA;
struct KUSER_SHARED_DATA {
	volatile ULONG TickCountLowDeprecated; // 0x0
	ULONG TickCountMultiplier; // 0x4
	volatile KSYSTEM_TIME InterruptTime; // 0x8
	volatile KSYSTEM_TIME SystemTime; // 0x14
	volatile KSYSTEM_TIME TimeZoneBias; // 0x20
	USHORT ImageNumberLow; // 0x2C
	USHORT ImageNumberHigh; // 0x2E

	// This is the path to the windows directory
	WCHAR NtSystemRoot[MAX_PATH]; // 0x30
	ULONG MaxStackTraceDepth; // 0x238
	ULONG CryptoExponent; // 0x23C
	ULONG TimeZoneId; // 0x240

	// Begin Server 2003+
	ULONG LargePageMinimum; // 0x244
	// End Server 2003+

	ULONG unreliable_members_1[6]; // 0x248

	// Begin Windows 10+
	ULONG NtBuildNumber; // 0x260
	// End Windows 10+

	NT_PRODUCT_TYPE NtProductType; // 0x264
	BOOLEAN ProductTypeIsValid; // 0x268

	// Begin Windows 8+
	BOOLEAN Reserved0[1]; // 0x269
	USHORT NativeProcessorArchitecture; // 0x26A
	// End Windows 8+

	ULONG NtMajorVersion; // 0x26C
	ULONG NtMinorVersion; // 0x270
	BOOLEAN ProcessorFeatures[0x40]; // 0x274

	// Begin Windows NT 4 specific (Still have values in later versions, but are almost useless with only 32 bits each)
	ULONG MmHighestUserAddress; // 0x2B4
	ULONG MmSystemRangeStart; // 0x2B8
	// End Windows NT 4 specific

	volatile ULONG TimeSlip; // 0x2BC
	ALTERNATIVE_ARCHITECTURE_TYPE AlternativeArchitecture; // 0x2C0

	// Begin Windows 10+
	ULONG BootId; // 0x2C4
	// End Windows 10+

	LARGE_INTEGER SystemExpirationDate; // 0x2C8
	ULONG SuiteMask; // 0x2D0
	BOOLEAN KdDebuggerEnabled; // 0x2D4
	union {
		UCHAR MitigationPolicies; // 0x2D5
		struct {
			UCHAR NXSupportPolicy : 2;
			// Begin Windows 8+
			UCHAR SEHValidationPolicy : 2;
			UCHAR CurDirDevicesSkippedForDlls : 2;
			UCHAR Reserved : 2;
			// End Windows 8+
		};
	};

	// Begin Windows 10+
	USHORT CyclesPerYield; // 0x2D6
	// End Windows 10+

	volatile ULONG ActiveConsoleId; // 0x2D8
	volatile ULONG DismountCount; // 0x2DC
	ULONG ComPlusPackage; // 0x2E0
	ULONG LastSystemRITEventTickCount; // 0x2E4
	ULONG NumberOfPhysicalPages; // 0x2E8
	BOOLEAN SafeBootMode; // 0x2EC
	UCHAR unreliable_member_2; // 0x2ED
	UCHAR Reserved12[2]; // 0x2EE
	ULONG unreliable_member_3; // 0x2F0
	ULONG DataFlagsPad[1]; // 0x2F4

	ULONGLONG TestRetInstruction; // 0x2F8
	ULONG unreliable_members_4[4]; // 0x300
	ULONGLONG SystemCallPad[2]; // 0x310

	union {
		volatile KSYSTEM_TIME TickCount; // 0x320
		volatile ULONG64 TickCountQuad; // 0x320
		struct {
			ULONG ReservedTickCountOverlay[3]; // 0x320
			ULONG TickCountPad[1]; // 0x32C
		};
	};
	ULONG Cookie; // 0x330
	ULONG CookiePad[1]; // 0x334
};

#define MM_SHARED_USER_DATA_VA (0x7FFE0000u)
#define USER_SHARED_DATA (*(const KUSER_SHARED_DATA *const)MM_SHARED_USER_DATA_VA)
