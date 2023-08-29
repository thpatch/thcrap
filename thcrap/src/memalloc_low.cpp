/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * x64 manipulation of virtual memory
  * that can be accessed with absolute encodings
  */

#include "thcrap.h"

typedef LONG NTSTATUS;

static NTSTATUS NTAPI NtAllocateVirtualMemoryShim(HANDLE ProcessHandle, PVOID *BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect);
static NTSTATUS NTAPI NtFreeVirtualMemoryShim(HANDLE ProcessHandle, PVOID *BaseAddress, PSIZE_T RegionSize, ULONG FreeType);
static ULONG NTAPI RtlNtStatusToDosErrorShim(NTSTATUS Status);

typedef decltype(&NtAllocateVirtualMemoryShim) NtAllocateVirtualMemoryPtr;
typedef decltype(&NtFreeVirtualMemoryShim) NtFreeVirtualMemoryPtr;
typedef decltype(&RtlNtStatusToDosErrorShim) RtlNtStatusToDosErrorPtr;

static NtAllocateVirtualMemoryPtr NtAllocateVirtualMemory = &NtAllocateVirtualMemoryShim;
static NtFreeVirtualMemoryPtr NtFreeVirtualMemory = &NtFreeVirtualMemoryShim;
static RtlNtStatusToDosErrorPtr RtlNtStatusToDosError = &RtlNtStatusToDosErrorShim;

static void resolve_nt_funcs();

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

static void resolve_nt_funcs() {
	const HMODULE ntdll_module = (HMODULE)GetModuleHandleA("ntdll.dll");
	NtAllocateVirtualMemory = (NtAllocateVirtualMemoryPtr)GetProcAddress(ntdll_module, "NtAllocateVirtualMemory");
	NtFreeVirtualMemory = (NtFreeVirtualMemoryPtr)GetProcAddress(ntdll_module, "NtFreeVirtualMemory");
	RtlNtStatusToDosError = (RtlNtStatusToDosErrorPtr)GetProcAddress(ntdll_module, "RtlNtStatusToDosError");
}

static constexpr size_t low_mem_zero_mask = 0x7FFFFFFF;
static constexpr uintptr_t null_region_size = 0x10000;
static constexpr uint32_t max_numa_node = 0x3F;

#define STATUS_SUCCESS					((DWORD)0x00000000)
#define STATUS_INVALID_PAGE_PROTECTION	((DWORD)0xC0000045)
#define INVALID_PARAMETER_VALUE 0x57

LPVOID VirtualAllocLowEx(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
	if (lpAddress && (uintptr_t)lpAddress < null_region_size) {
		SetLastError(INVALID_PARAMETER_VALUE);
		return NULL;
	}
	flAllocationType &= ~max_numa_node; // Make sure we don't do anything weird anyway...
	NTSTATUS status = NtAllocateVirtualMemory(hProcess, &lpAddress, low_mem_zero_mask, &dwSize, flAllocationType, flProtect);
	if (status < STATUS_SUCCESS) {
		return lpAddress;
	} else {
		SetLastError(RtlNtStatusToDosError(status));
		return NULL;
	}
}

LPVOID VirtualAllocLow(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
	return VirtualAllocLowEx((HANDLE)0, lpAddress, dwSize, flAllocationType, flProtect);
}

BOOL VirtualFreeLowEx(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
	if ((dwFreeType & MEM_RELEASE) && !dwSize) {
		SetLastError(INVALID_PARAMETER_VALUE);
		return false;
	}
	NTSTATUS status = NtFreeVirtualMemory(hProcess, &lpAddress, &dwSize, dwFreeType);
	if (status < STATUS_SUCCESS) {
		return TRUE;
	} else {
		SetLastError(RtlNtStatusToDosError(status));
		return FALSE;
	}
}

BOOL VirtualFreeLow(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
	return VirtualFreeLowEx((HANDLE)0, lpAddress, dwSize, dwFreeType);
}
