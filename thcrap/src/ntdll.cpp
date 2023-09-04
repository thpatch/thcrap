/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * ntdll shim functions
  */

#include "thcrap.h"

static void resolve_nt_funcs();

static NTSTATUS NTAPI NtQueryInformationProcessShim(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength) {
	resolve_nt_funcs();
	return NtQueryInformationProcess(ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);
}
NtQueryInformationProcessPtr NtQueryInformationProcess = &NtQueryInformationProcessShim;

#if TH_X64
static NTSTATUS NTAPI NtAllocateVirtualMemoryShim(HANDLE ProcessHandle, PVOID *BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect) {
	resolve_nt_funcs();
	return NtAllocateVirtualMemory(ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect);
}
static NTSTATUS NTAPI NtFreeVirtualMemoryShim(HANDLE ProcessHandle, PVOID *BaseAddress, PSIZE_T RegionSize, ULONG FreeType) {
	resolve_nt_funcs();
	return NtFreeVirtualMemory(ProcessHandle, BaseAddress, RegionSize, FreeType);
}
static ULONG NTAPI RtlNtStatusToDosErrorShim(NTSTATUS Status) {
	resolve_nt_funcs();
	return RtlNtStatusToDosError(Status);
}

NtAllocateVirtualMemoryPtr NtAllocateVirtualMemory = &NtAllocateVirtualMemoryShim;
NtFreeVirtualMemoryPtr NtFreeVirtualMemory = &NtFreeVirtualMemoryShim;
RtlNtStatusToDosErrorPtr RtlNtStatusToDosError = &RtlNtStatusToDosErrorShim;
#endif

static void resolve_nt_funcs() {
	const HMODULE ntdll_module = (HMODULE)GetModuleHandleA("ntdll.dll");
	NtQueryInformationProcess = (NtQueryInformationProcessPtr)GetProcAddress(ntdll_module, "NtQueryInformationProcess");
#if TH_X64
	NtAllocateVirtualMemory = (NtAllocateVirtualMemoryPtr)GetProcAddress(ntdll_module, "NtAllocateVirtualMemory");
	NtFreeVirtualMemory = (NtFreeVirtualMemoryPtr)GetProcAddress(ntdll_module, "NtFreeVirtualMemory");
	RtlNtStatusToDosError = (RtlNtStatusToDosErrorPtr)GetProcAddress(ntdll_module, "RtlNtStatusToDosError");
#endif
}
