/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * x64 manipulation of virtual memory
  * that can be accessed with absolute encodings
  *
  * On x32 these are exact duplicates of
  * the regular virtual memory functions
  */

#pragma once

#if TH_X64
LPVOID VirtualAllocLowEx(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
LPVOID VirtualAllocLow(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
BOOL VirtualFreeLowEx(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
BOOL VirtualFreeLow(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
#else
static TH_FORCEINLINE LPVOID VirtualAllocLowEx(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
	return VirtualAllocEx(hProcess, lpAddress, dwSize, flAllocationType, flProtect);
}
static TH_FORCEINLINE LPVOID VirtualAllocLow(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
	return VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
}
static TH_FORCEINLINE BOOL VirtualFreeLowEx(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
	return VirtualFreeEx(hProcess, lpAddress, dwSize, dwFreeType);
}
static TH_FORCEINLINE BOOL VirtualFreeLow(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
	return VirtualFree(lpAddress, dwSize, dwFreeType);
}
#endif
