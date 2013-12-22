/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Direct memory and Import Address Table detouring
  */

#include "thcrap.h"

BOOL VirtualCheckRegion(const void *ptr, const size_t len)
{
	MEMORY_BASIC_INFORMATION mbi;
	if(VirtualQuery(ptr, &mbi, sizeof(MEMORY_BASIC_INFORMATION))) {
		return ((size_t)mbi.BaseAddress + mbi.RegionSize) >= ((size_t)ptr + len);
	}
	return FALSE;
}

BOOL VirtualCheckCode(const void *ptr)
{
	return VirtualCheckRegion(ptr, 1);
}

int PatchRegionNoCheck(void *ptr, void *New, size_t len)
{
	MEMORY_BASIC_INFORMATION mbi;
	DWORD oldProt;
	int ret = 0;

	VirtualQuery(ptr, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
	VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &oldProt);
	memcpy(ptr, New, len);
	VirtualProtect(mbi.BaseAddress, mbi.RegionSize, oldProt, &oldProt);
	return ret;
}

int PatchRegion(void *ptr, void *Prev, void *New, size_t len)
{
	MEMORY_BASIC_INFORMATION mbi;
	DWORD oldProt;
	int ret = 0;

	VirtualQuery(ptr, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
	VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &oldProt);
	if(!memcmp(ptr, Prev, len)) {
		memcpy(ptr, New, len);
		ret = 1;
	}
	VirtualProtect(mbi.BaseAddress, mbi.RegionSize, oldProt, &oldProt);
	return ret;
}

int PatchRegionEx(HANDLE hProcess, void *ptr, void *Prev, void *New, size_t len)
{
	MEMORY_BASIC_INFORMATION mbi;
	DWORD oldProt;
	VLA(BYTE, old_val, len);
	SIZE_T byte_ret;

	VirtualQueryEx(hProcess, ptr, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
	VirtualProtectEx(hProcess, mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &oldProt);
	ReadProcessMemory(hProcess, ptr, old_val, len, &byte_ret);
	if(!memcmp(old_val, Prev, len)) {
		WriteProcessMemory(hProcess, ptr, New, len, &byte_ret);
	}
	VirtualProtectEx(hProcess, mbi.BaseAddress, mbi.RegionSize, oldProt, &oldProt);
	VLA_FREE(old_val);
	return byte_ret != len;
}

int PatchBYTE(void *ptr, BYTE Prev, BYTE Val)
{
	return PatchRegion(ptr, (void*)Prev, (void*)Val, sizeof(BYTE));
}

int PatchDWORD(void *ptr, DWORD Prev, DWORD Val)
{
	return PatchRegion(ptr, (void*)Prev, (void*)Val, sizeof(DWORD));
}

int PatchFLOAT(void *ptr, FLOAT Prev, FLOAT Val)
{
	return PatchRegion(ptr, &Prev, &Val, sizeof(FLOAT));
}

int PatchBYTEEx(HANDLE hProcess, void *ptr, BYTE Prev, BYTE Val)
{
	return PatchRegionEx(hProcess, ptr, (void*)Prev, (void*)Val, sizeof(BYTE));
}

int PatchDWORDEx(HANDLE hProcess, void *ptr, DWORD Prev, DWORD Val)
{
	return PatchRegionEx(hProcess, ptr, (void*)Prev, (void*)Val, sizeof(DWORD));
}

int PatchFLOATEx(HANDLE hProcess, void *ptr, FLOAT Prev, FLOAT Val)
{
	return PatchRegionEx(hProcess, ptr, &Prev, &Val, sizeof(FLOAT));
}

/// Import Address Table detouring
/// ==============================

/// Low-level
/// ---------
int func_detour(PIMAGE_THUNK_DATA pThunk, const void *new_ptr)
{
	MEMORY_BASIC_INFORMATION mbi;
	DWORD oldProt;
	VirtualQuery(&pThunk->u1.Function, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
	VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &oldProt);
	pThunk->u1.Function = (DWORD)new_ptr;
	VirtualProtect(mbi.BaseAddress, mbi.RegionSize, oldProt, &oldProt);
	return 1;
}

int func_detour_by_name(HMODULE hMod, PIMAGE_THUNK_DATA pOrigFirstThunk, PIMAGE_THUNK_DATA pImpFirstThunk, const char *old_func, const void *new_ptr)
{
	PIMAGE_THUNK_DATA pOT, pIT;

	if(!new_ptr || !VirtualCheckCode(new_ptr)) {
		return 0;
	}
	for(pOT = pOrigFirstThunk, pIT = pImpFirstThunk; pOT->u1.Function; pOT++, pIT++) {
		if(!(pOT->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
			PIMAGE_IMPORT_BY_NAME pByName = (PIMAGE_IMPORT_BY_NAME)((DWORD)hMod + pOT->u1.AddressOfData);
			if(pByName->Name[0] == '\0') {
				return 0;
			}
			if(!stricmp(old_func, (char*)pByName->Name)) {
				return func_detour(pIT, new_ptr);
			}
		}
	}
	// Function not found
	return 0;
}

int func_detour_by_ptr(PIMAGE_THUNK_DATA pImpFirstThunk, const void *old_ptr, const void *new_ptr)
{
	PIMAGE_THUNK_DATA Thunk;
	if(!new_ptr || !VirtualCheckCode(new_ptr)) {
		return 0;
	}
	for(Thunk = pImpFirstThunk; Thunk->u1.Function; Thunk++) {
		if((DWORD*)Thunk->u1.Function == (DWORD*)old_ptr) {
			return func_detour(Thunk, new_ptr);
		}
	}
	// Function not found
	return 0;
}
/// ---------

/// High-level
/// ----------
void iat_detour_set(iat_detour_t *detour, const char *old_func, const void *old_ptr, const void *new_ptr)
{
	detour->old_func = old_func;
	detour->old_ptr = old_ptr;
	detour->new_ptr = new_ptr;
}

int iat_detour_funcs(HMODULE hMod, const char *dll_name, iat_detour_t *detour, const size_t detour_count)
{
	PIMAGE_IMPORT_DESCRIPTOR ImpDesc;
	PIMAGE_THUNK_DATA pOrigThunk;
	PIMAGE_THUNK_DATA pImpThunk;
	int ret = detour_count;
	UINT c;

	ImpDesc = GetDllImportDesc(hMod, dll_name);
	if(!ImpDesc) {
		return -1;
	}
	pOrigThunk = (PIMAGE_THUNK_DATA)((DWORD)hMod + (DWORD)ImpDesc->OriginalFirstThunk);
	pImpThunk  = (PIMAGE_THUNK_DATA)((DWORD)hMod + (DWORD)ImpDesc->FirstThunk);

	log_printf("Detouring DLL functions (%s)...\n", dll_name);

	// We _only_ detour by comparing exported names.
	// Has the advantages that we can override any existing patches,
	// and that it works on Win9x too (as if that matters).

	for(c = 0; c < detour_count; c++) {
		DWORD local_ret = func_detour_by_name(hMod, pOrigThunk, pImpThunk, detour[c].old_func, detour[c].new_ptr);
		log_printf(
			"(%2d/%2d) %s... %s\n",
			c + 1, detour_count, detour[c].old_func, local_ret ? "OK" : "not found"
		);
		ret -= local_ret;
	}
	return ret;
}

int iat_detour_funcs_var(HMODULE hMod, const char *dll_name, const size_t func_count, ...)
{
	HMODULE hDll;
	va_list va;
	VLA(iat_detour_t, iat_detour, func_count);
	size_t i;
	int ret;

	if(!dll_name || !hMod) {
		return -1;
	}
	hDll = GetModuleHandleA(dll_name);
	if(!hDll) {
		return -2;
	}
	va_start(va, func_count);
	for(i = 0; i < func_count; i++) {
		iat_detour[i].old_func = va_arg(va, const char*);
		iat_detour[i].new_ptr = va_arg(va, const void*);
		iat_detour[i].old_ptr = GetProcAddress(hDll, iat_detour[i].old_func);
	}
	va_end(va);
	ret = iat_detour_funcs(hMod, dll_name, iat_detour, func_count);
	VLA_FREE(iat_detour);
	return ret;
}
/// ----------

/// =============================
