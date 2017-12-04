/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Direct memory patching and Import Address Table detouring.
  */

#include "thcrap.h"

static json_t *detours = NULL;

BOOL VirtualCheckRegion(const void *ptr, const size_t len)
{
	MEMORY_BASIC_INFORMATION mbi;
	if(VirtualQuery(ptr, &mbi, sizeof(MEMORY_BASIC_INFORMATION))) {
		auto page_end = (size_t)mbi.BaseAddress + mbi.RegionSize;
		return
			(~mbi.Protect & PAGE_NOACCESS)
			&& (page_end >= ((size_t)ptr + len));
	}
	return FALSE;
}

BOOL VirtualCheckCode(const void *ptr)
{
	return VirtualCheckRegion(ptr, 1);
}

int PatchRegion(void *ptr, const void *Prev, const void *New, size_t len)
{
	DWORD oldProt;
	int ret = 0;

	if(!VirtualCheckRegion(ptr, len)) {
		return ret;
	}
	VirtualProtect(ptr, len, PAGE_READWRITE, &oldProt);
	if(Prev ? !memcmp(ptr, Prev, len) : 1) {
		memcpy(ptr, New, len);
		ret = 1;
	}
	VirtualProtect(ptr, len, oldProt, &oldProt);
	return ret;
}

int PatchRegionEx(HANDLE hProcess, void *ptr, const void *Prev, const void *New, size_t len)
{
	MEMORY_BASIC_INFORMATION mbi;
	DWORD oldProt;
	VLA(BYTE, old_val, len);
	SIZE_T byte_ret;

	VirtualQueryEx(hProcess, ptr, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
	VirtualProtectEx(hProcess, mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &oldProt);
	ReadProcessMemory(hProcess, ptr, old_val, len, &byte_ret);
	if(Prev ? !memcmp(old_val, Prev, len) : 1) {
		WriteProcessMemory(hProcess, ptr, New, len, &byte_ret);
	}
	VirtualProtectEx(hProcess, mbi.BaseAddress, mbi.RegionSize, oldProt, &oldProt);
	VLA_FREE(old_val);
	return byte_ret != len;
}

/// Import Address Table detouring
/// ==============================

/// Low-level
/// ---------
int func_detour(PIMAGE_THUNK_DATA pThunk, const void *new_ptr)
{
	return PatchRegion(&pThunk->u1.Function, NULL, &new_ptr, sizeof(new_ptr));
}

int func_detour_by_name(HMODULE hMod, PIMAGE_THUNK_DATA pOrigFirstThunk, PIMAGE_THUNK_DATA pImpFirstThunk, const iat_detour_t *detour)
{
	if(
		detour && detour->old_func && detour->new_ptr
		&& VirtualCheckCode(detour->new_ptr)
	) {
		PIMAGE_THUNK_DATA pOT = pOrigFirstThunk;
		PIMAGE_THUNK_DATA pIT = pImpFirstThunk;
		for(; pOT->u1.Function; pOT++, pIT++) {
			PIMAGE_IMPORT_BY_NAME pByName;
			if(!(pOT->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
				pByName = (PIMAGE_IMPORT_BY_NAME)((DWORD)hMod + pOT->u1.AddressOfData);
				if(pByName->Name[0] == '\0') {
					return 0;
				}
				if(!stricmp(detour->old_func, (char*)pByName->Name)) {
					return func_detour(pIT, detour->new_ptr);
				}
			}
		}
	}
	// Function not found
	return 0;
}

int func_detour_by_ptr(PIMAGE_THUNK_DATA pImpFirstThunk, const iat_detour_t *detour)
{
	if(detour && detour->new_ptr && VirtualCheckCode(detour->new_ptr)) {
		PIMAGE_THUNK_DATA Thunk;
		for(Thunk = pImpFirstThunk; Thunk->u1.Function; Thunk++) {
			if((DWORD*)Thunk->u1.Function == (DWORD*)detour->old_ptr) {
				return func_detour(Thunk, detour->new_ptr);
			}
		}
	}
	// Function not found
	return 0;
}
/// ---------

/// High-level
/// ----------
int iat_detour_func(HMODULE hMod, PIMAGE_IMPORT_DESCRIPTOR pImpDesc, const iat_detour_t *detour)
{
	PIMAGE_THUNK_DATA pOT = NULL;
	PIMAGE_THUNK_DATA pIT = NULL;
	if(hMod && pImpDesc && detour) {
		pIT = (PIMAGE_THUNK_DATA)((DWORD)hMod + pImpDesc->FirstThunk);

		// We generally detour by comparing exported names. This has the
		// advantage that we can override any existing patches, and that
		// it works on Win9x too (as if that matters). However, in case we lack
		// a pointer to the OriginalFirstThunk, this is not possible, so we have
		// to detour by comparing pointers then.

		if(pImpDesc->OriginalFirstThunk) {
			pOT = (PIMAGE_THUNK_DATA)((DWORD)hMod + pImpDesc->OriginalFirstThunk);
			return func_detour_by_name(hMod, pOT, pIT, detour);
		} else {
			return func_detour_by_ptr(pIT, detour);
		}
	}
	return -1;
}
/// ----------

/// Detour chaining
/// ---------------
json_t* detour_get_create(const char *dll_name)
{
	json_t *ret = NULL;
	STRLWR_DEC(dll_name);
	STRLWR_CONV(dll_name);
	if(!detours) {
		detours = json_object();
	}
	ret = json_object_get_create(detours, dll_name_lower, JSON_OBJECT);
	VLA_FREE(dll_name_lower);

	return ret;
}

int detour_chain(const char *dll_name, int return_old_ptrs, ...)
{
	int ret = 0;
	json_t *dll = detour_get_create(dll_name);
	const char *func_name = NULL;
	va_list va;

	va_start(va, return_old_ptrs);
	while(func_name = va_arg(va, const char*)) {
		FARPROC *old_ptr = NULL;
		FARPROC chain_ptr;
		const void *func_ptr = va_arg(va, const void*);
		if(
			return_old_ptrs
			&& (old_ptr = va_arg(va, FARPROC*))
			&& (chain_ptr = (FARPROC)json_object_get_hex(dll, func_name))
			&& (chain_ptr != func_ptr)
		) {
			*old_ptr = chain_ptr;
		}
		json_object_set_new(dll, func_name, json_integer((size_t)func_ptr));
	}
	va_end(va);
	return ret;
}

int detour_chain_w32u8(const w32u8_dll_t *dll)
{
	const w32u8_pair_t *pair = NULL;
	json_t *detours_dll = NULL;

	if(!dll || !dll->name || !dll->funcs) {
		return -1;
	}
	detours_dll = detour_get_create(dll->name);
	pair = dll->funcs;
	while(pair && pair->ansi_name && pair->utf8_ptr) {
		json_object_set_new(
			detours_dll, pair->ansi_name, json_integer((size_t)pair->utf8_ptr)
		);
		pair++;
	}
	return 0;
}

int iat_detour_apply(HMODULE hMod)
{
	int ret = 0;
	PIMAGE_IMPORT_DESCRIPTOR pImpDesc;

	pImpDesc = (PIMAGE_IMPORT_DESCRIPTOR)GetNtDataDirectory(hMod, IMAGE_DIRECTORY_ENTRY_IMPORT);
	if(!pImpDesc) {
		return ret;
	}

	while(pImpDesc->Name) {
		json_t *funcs;
		size_t func_count;
		char *dll_name = (char*)((DWORD)hMod + (DWORD)pImpDesc->Name);
		HMODULE hDll = GetModuleHandleA(dll_name);
		STRLWR_DEC(dll_name);

		STRLWR_CONV(dll_name);
		funcs = json_object_get(detours, dll_name_lower);
		func_count = json_object_size(funcs);

		if(hDll && func_count) {
			const char *func_name;
			json_t *ptr;
			size_t i = 0;

			log_printf("Detouring DLL functions (%s)...\n", dll_name_lower);

			json_object_foreach(funcs, func_name, ptr) {
				iat_detour_t detour;
				int local_ret;

				detour.old_func = func_name;
				detour.old_ptr = GetProcAddress(hDll, detour.old_func);
				detour.new_ptr = (FARPROC)json_integer_value(ptr);

				local_ret = iat_detour_func(hMod, pImpDesc, &detour);

				log_printf(
					"(%2d/%2d) %s... %s\n",
					i + 1, func_count, detour.old_func,
					local_ret ? "OK" : "not found"
				);
				i++;
			}
		}
		pImpDesc++;
	}
	return ret;
}

FARPROC detour_top(const char *dll_name, const char *func_name, FARPROC fallback)
{
	json_t *funcs = json_object_get_create(detours, dll_name, JSON_OBJECT);
	FARPROC ret = (FARPROC)json_object_get_hex(funcs, func_name);
	return ret ? ret : fallback;
}

int vtable_detour(void **vtable, vtable_detour_t *det, size_t det_count)
{
	assert(vtable);
	assert(det);

	if(det_count == 0) {
		return 0;
	}

	DWORD old_prot;
	int replaced = 0;
	size_t i;

	// VirtualProtect() is infamously slow, so...
	size_t lowest = (size_t)-1;
	size_t highest = 0;

	for(i = 0; i < det_count; i++) {
		lowest = min(lowest, det[i].index);
		highest = max(highest, det[i].index);
	}

	size_t bytes_to_lock = ((highest + 1) - lowest) * sizeof(void*);

	VirtualProtect(vtable + lowest, bytes_to_lock, PAGE_READWRITE, &old_prot);
	for(i = 0; i < det_count; i++) {
		auto& cur = det[i];

		bool replace = (cur.old_func == nullptr) || (*cur.old_func == nullptr);
		bool set_old = (cur.old_func != nullptr) && (*cur.old_func == nullptr);

		if(set_old) {
			*cur.old_func = vtable[cur.index];
		}
		if(replace) {
			vtable[cur.index] = cur.new_func;
			replaced++;
		}
	}
	VirtualProtect(vtable + lowest, bytes_to_lock, old_prot, &old_prot);
	return replaced;
}

void detour_exit(void)
{
	detours = json_decref_safe(detours);
}
/// ----------

/// =============================
