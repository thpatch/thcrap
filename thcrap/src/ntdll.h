/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * ntdll shim functions
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

