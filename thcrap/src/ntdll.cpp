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

static NTSTATUS NTAPI NtQuerySystemInformationShim(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength) {
	resolve_nt_funcs();
	return NtQuerySystemInformation(SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);
}
NtQuerySystemInformationPtr NtQuerySystemInformation = &NtQuerySystemInformationShim;

static NTSTATUS NTAPI NtQueryInformationProcessShim(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength) {
	resolve_nt_funcs();
	return NtQueryInformationProcess(ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);
}
NtQueryInformationProcessPtr NtQueryInformationProcess = &NtQueryInformationProcessShim;

/*
static NTSTATUS NTAPI NtQueryInformationThreadShim(HANDLE ThreadHandle, THREADINFOCLASS ThreadInformationClass, PVOID ThreadInformation, ULONG ThreadInformationLength, PULONG ReturnLength) {
	resolve_nt_funcs();
	return NtQueryInformationThread(ThreadHandle, ThreadInformationClass, ThreadInformation, ThreadInformationLength, ReturnLength);
}
NtQueryInformationThreadPtr NtQueryInformationThread = &NtQueryInformationThreadShim;
*/

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
	const HMODULE ntdll_module = (HMODULE)GetModuleHandleW(L"ntdll.dll");
	NtQuerySystemInformation = (NtQuerySystemInformationPtr)GetProcAddress(ntdll_module, "NtQuerySystemInformation");
	NtQueryInformationProcess = (NtQueryInformationProcessPtr)GetProcAddress(ntdll_module, "NtQueryInformationProcess");
	//NtQueryInformationThread = (NtQueryInformationThreadPtr)GetProcAddress(ntdll_module, "NtQueryInformationThread");
#if TH_X64
	NtAllocateVirtualMemory = (NtAllocateVirtualMemoryPtr)GetProcAddress(ntdll_module, "NtAllocateVirtualMemory");
	NtFreeVirtualMemory = (NtFreeVirtualMemoryPtr)GetProcAddress(ntdll_module, "NtFreeVirtualMemory");
	RtlNtStatusToDosError = (RtlNtStatusToDosErrorPtr)GetProcAddress(ntdll_module, "RtlNtStatusToDosError");
#endif
}

// It was either this or CreateToolhelp32Snapshot
TH_CALLER_FREE HANDLE* get_all_threads(DWORD access) {
	HANDLE* handles = NULL;

	PROCESS_BASIC_INFORMATION pbi;
	if (NtGetProcessBasicInfomation(CURRENT_PROCESS_PSEUDO_HANDLE, pbi)) {

		ULONG process_list_size;

		NTSTATUS status = NtQuerySystemInformation(SystemExtendedProcessInformation, NULL, 0, &process_list_size);
		if (status == STATUS_INFO_LENGTH_MISMATCH || status == STATUS_BUFFER_TOO_SMALL) {
			SYSTEM_PROCESS_INFORMATION* processes = (SYSTEM_PROCESS_INFORMATION*)malloc(process_list_size);
			if (processes) {
		retry:
				status = NtQuerySystemInformation(SystemExtendedProcessInformation, processes, process_list_size, &process_list_size);
				if (status == STATUS_SUCCESS) {
					SYSTEM_PROCESS_INFORMATION* process = processes;
					for (;;) {

						if (
							// Find our process in the list.
							// Why isn't there an API for only one process?
							process->UniqueProcessId == pbi.UniqueProcessId
						) {
							size_t thread_count = process->NumberOfThreads;
							handles = (HANDLE*)malloc((thread_count + 1) * sizeof(HANDLE));
							if (handles) {
								size_t handles_opened = 0;
								for (size_t i = 0; i != thread_count; ++i) {
									HANDLE handle = OpenThread(access, false, (DWORD)process->threads[i].ClientId.UniqueThread);
									if (handle) {
										handles[handles_opened++] = handle;
									}
								}
								handles[handles_opened] = NULL;
							}
							break;
						}

						uint32_t next_offset = process->NextEntryOffset;
						if (!next_offset) break;
						process = (SYSTEM_PROCESS_INFORMATION*)((uintptr_t)process + next_offset);
					}
				}
				else if (status == STATUS_INFO_LENGTH_MISMATCH || status == STATUS_BUFFER_TOO_SMALL) {
					// Process/thread count could change between calls...
					SYSTEM_PROCESS_INFORMATION* new_processes = (SYSTEM_PROCESS_INFORMATION*)realloc(processes, process_list_size);
					if (new_processes) {
						processes = new_processes;
						goto retry;
					}
				}
				free(processes);
			}
		}
	}
	return handles;
}
