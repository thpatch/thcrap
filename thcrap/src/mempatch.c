/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Direct memory and Import Address Table patching
  */

#include "thcrap.h"

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

/// PE structures
/// -------------
// Adapted from http://forum.sysinternals.com/createprocess-api-hook_topic13138.html
PIMAGE_NT_HEADERS WINAPI GetNtHeader(HMODULE hMod)
{
	PIMAGE_DOS_HEADER pDosH;
	PIMAGE_NT_HEADERS pNTH;

	if(!hMod) {
		return 0;
	}
	// Get DOS Header
	pDosH = (PIMAGE_DOS_HEADER) hMod;

	// Verify that the PE is valid by checking e_magic's value and DOS Header size
	if(IsBadReadPtr(pDosH, sizeof(IMAGE_DOS_HEADER))) {
		return 0;
	}
	if(pDosH->e_magic != IMAGE_DOS_SIGNATURE) {
		return 0;
	}
	// Find the NT Header by using the offset of e_lfanew value from hMod
	pNTH = (PIMAGE_NT_HEADERS)((DWORD)pDosH + (DWORD)pDosH->e_lfanew);

	// Verify that the NT Header is correct
	if(IsBadReadPtr(pNTH, sizeof(IMAGE_NT_HEADERS))) {
		return 0;
	}
	if(pNTH->Signature != IMAGE_NT_SIGNATURE) {
		return 0;
	}
	return pNTH;
}

PIMAGE_IMPORT_DESCRIPTOR WINAPI GetDllImportDesc(HMODULE hMod, const char *DLLName)
{
	PIMAGE_NT_HEADERS pNTH;
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc;

	if(!hMod || !DLLName) {
		return 0;
	}
	pNTH = GetNtHeader(hMod);
	if(!pNTH) {
		return NULL;
	}
	// iat patching
	pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD)hMod +
		(DWORD)(pNTH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress));

	if(pImportDesc == (PIMAGE_IMPORT_DESCRIPTOR)pNTH) {
		return NULL;
	}
	while(pImportDesc->Name) {
		// pImportDesc->Name gives the name of the module
		char *name = (char*)((DWORD)hMod + (DWORD)pImportDesc->Name);
		// stricmp returns 0 if strings are equal, case insensitive
		if(stricmp(name, DLLName) == 0) {
			return pImportDesc;
		}
		++pImportDesc;
	}
	return NULL;
}

PIMAGE_EXPORT_DIRECTORY WINAPI GetDllExportDesc(HMODULE hMod)
{
	PIMAGE_NT_HEADERS pNTH;
	if(!hMod) {
		return NULL;
	}
	pNTH = GetNtHeader(hMod);
	if(!pNTH) {
		return NULL;
	}
	return (PIMAGE_EXPORT_DIRECTORY)((DWORD)hMod +
		(DWORD)(pNTH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress));
}

PIMAGE_SECTION_HEADER WINAPI GetSectionHeader(HMODULE hMod, const char *section_name)
{
	PIMAGE_NT_HEADERS pNTH;
	PIMAGE_SECTION_HEADER pSH;
	WORD c;

	if(!hMod || !section_name) {
		return 0;
	}
	pNTH = GetNtHeader(hMod);
	if(!pNTH) {
		return NULL;
	}
	// OptionalHeader position + SizeOfOptionalHeader = Section headers
	pSH = (PIMAGE_SECTION_HEADER)((DWORD)(&pNTH->OptionalHeader) + (DWORD)pNTH->FileHeader.SizeOfOptionalHeader);

	if(IsBadReadPtr(pSH, sizeof(IMAGE_SECTION_HEADER) * pNTH->FileHeader.NumberOfSections)) {
		return 0;
	}
	// Search
	for(c = 0; c < pNTH->FileHeader.NumberOfSections; c++) {
		if(strncmp(pSH->Name, section_name, 8) == 0) {
			return pSH;
		}
		++pSH;
	}
	return NULL;
}

/// Import Address Table patching
/// =============================

/// Low-level
/// ---------
int func_patch(PIMAGE_THUNK_DATA pThunk, const void *new_ptr)
{
	MEMORY_BASIC_INFORMATION mbi;
	DWORD oldProt;
	VirtualQuery(&pThunk->u1.Function, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
	VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &oldProt);
	pThunk->u1.Function = (DWORD)new_ptr;
	VirtualProtect(mbi.BaseAddress, mbi.RegionSize, oldProt, &oldProt);
	return 1;
}

int func_patch_by_name(HMODULE hMod, PIMAGE_THUNK_DATA pOrigFirstThunk, PIMAGE_THUNK_DATA pImpFirstThunk, const char *old_func, const void *new_ptr)
{
	PIMAGE_THUNK_DATA pOT, pIT;

	// Verify that the newFunc is valid
	if(!new_ptr || IsBadCodePtr((FARPROC)new_ptr)) {
		return 0;
	}
	for(pOT = pOrigFirstThunk, pIT = pImpFirstThunk; pOT->u1.Function; pOT++, pIT++) {
		if((pOT->u1.Ordinal & IMAGE_ORDINAL_FLAG) != IMAGE_ORDINAL_FLAG) {
			PIMAGE_IMPORT_BY_NAME pByName = (PIMAGE_IMPORT_BY_NAME)((DWORD)hMod+(DWORD)(pOT->u1.AddressOfData));
            if(pByName->Name[0] == '\0') {
                return 0;
            }
            if(!stricmp(old_func, (char*)pByName->Name)) {
				return func_patch(pIT, new_ptr);
			}
		}
	}
	// Function not found
	return 0;
}

int func_patch_by_ptr(PIMAGE_THUNK_DATA pImpFirstThunk, const void *old_ptr, const void *new_ptr)
{
	PIMAGE_THUNK_DATA Thunk;

	// Verify that the new pointer is valid
	if(!new_ptr || IsBadCodePtr((FARPROC)new_ptr)) {
		return 0;
	}
	for(Thunk = pImpFirstThunk; Thunk->u1.Function; Thunk++) {
		if((DWORD*)Thunk->u1.Function == (DWORD*)old_ptr) {
			return func_patch(Thunk, new_ptr);
		}
	}
	// Function not found
	return 0;
}
/// ---------

/// High-level
/// ----------
void iat_patch_set(iat_patch_t *patch, const char *old_func, const void *old_ptr, const void *new_ptr)
{
	patch->old_func = old_func;
	patch->old_ptr = old_ptr;
	patch->new_ptr = new_ptr;
}

int iat_patch_funcs(HMODULE hMod, const char *dll_name, iat_patch_t *patch, const size_t patch_count)
{
	PIMAGE_IMPORT_DESCRIPTOR ImpDesc;
	PIMAGE_THUNK_DATA	pOrigThunk;
	PIMAGE_THUNK_DATA	pImpThunk;
	int ret = patch_count;
	UINT c;

	ImpDesc = GetDllImportDesc(hMod, dll_name);
	if(!ImpDesc) {
		return -1;
	}
	pOrigThunk = (PIMAGE_THUNK_DATA)((DWORD)hMod + (DWORD)ImpDesc->OriginalFirstThunk);
	pImpThunk  = (PIMAGE_THUNK_DATA)((DWORD)hMod + (DWORD)ImpDesc->FirstThunk);

	log_printf("Patching DLL exports (%s)...\n", dll_name);

	// We _only_ patch by comparing exported names.
	// Has the advantages that we can override any existing patches,
	// and that it works on Win9x too (as if that matters).

	for(c = 0; c < patch_count; c++) {
		DWORD local_ret = func_patch_by_name(hMod, pOrigThunk, pImpThunk, patch[c].old_func, patch[c].new_ptr);
		log_printf(
			"(%2d/%2d) %s... %s\n",
			c + 1, patch_count, patch[c].old_func, local_ret ? "OK" : "not found"
		);
		ret -= local_ret;
	}
	return ret;
}

int iat_patch_funcs_var(HMODULE hMod, const char *dll_name, const size_t func_count, ...)
{
	HMODULE hDll;
	va_list va;
	VLA(iat_patch_t, iat_patch, func_count);
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
		iat_patch[i].old_func = va_arg(va, const char*);
		iat_patch[i].new_ptr = va_arg(va, const void*);
		iat_patch[i].old_ptr = GetProcAddress(hDll, iat_patch[i].old_func);
	}
	va_end(va);
	ret = iat_patch_funcs(hMod, dll_name, iat_patch, func_count);
	VLA_FREE(iat_patch);
	return ret;
}
/// ----------

/// =============================
