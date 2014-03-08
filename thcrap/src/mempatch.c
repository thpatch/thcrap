/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Direct memory patching and Import Address Table detouring.
  */

#include "thcrap.h"

// Reverse variable argument list macros for detour_next()
#define va_rev_start(ap,v,num) (\
	ap = (va_list)_ADDRESSOF(v) + _INTSIZEOF(v) + (num + 1) * sizeof(int) \
)
#define va_rev_arg(ap,t) ( \
	*(t *)((ap -= _INTSIZEOF(t)) - _INTSIZEOF(t)) \
)

static json_t *detours = NULL;

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

int PatchRegion(void *ptr, const void *Prev, const void *New, size_t len)
{
	MEMORY_BASIC_INFORMATION mbi;
	DWORD oldProt;
	int ret = 0;

	VirtualQuery(ptr, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
	VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &oldProt);
	if(Prev ? !memcmp(ptr, Prev, len) : 1) {
		memcpy(ptr, New, len);
		ret = 1;
	}
	VirtualProtect(mbi.BaseAddress, mbi.RegionSize, oldProt, &oldProt);
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
void iat_detour_set(iat_detour_t *detour, const char *old_func, const void *old_ptr, const void *new_ptr)
{
	detour->old_func = old_func;
	detour->old_ptr = old_ptr;
	detour->new_ptr = new_ptr;
}

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

int iat_detour_funcs(HMODULE hMod, const char *dll_name, iat_detour_t *detour, const size_t detour_count)
{
	PIMAGE_IMPORT_DESCRIPTOR pImpDesc = GetDllImportDesc(hMod, dll_name);
	int ret = detour_count;
	UINT c;

	if(!pImpDesc) {
		return -1;
	}

	log_printf("Detouring DLL functions (%s)...\n", dll_name);

	for(c = 0; c < detour_count; c++) {
		int local_ret = iat_detour_func(hMod, pImpDesc, &detour[c]);
		log_printf(
			"(%2d/%2d) %s... %s\n",
			c + 1, detour_count, detour[c].old_func, local_ret ? "OK" : "not found"
		);
		ret -= local_ret;
	}
	return ret;
}

int detour_cache_add(const char *dll_name, const size_t func_count, ...)
{
	int ret = 0;
	json_t *dll = NULL;
	va_list va;
	size_t i;

	if(!dll_name) {
		return -1;
	}
	if(!detours) {
		detours = json_object();
	}
	dll = json_object_get_create(detours, dll_name, JSON_OBJECT);
	va_start(va, func_count);
	for(i = 0; i < func_count; i++) {
		const char *func_name = va_arg(va, const char*);
		const void *func_ptr = va_arg(va, const void*);
		json_t *func = json_object_get_create(dll, func_name, JSON_ARRAY);
		ret += json_array_insert_new(func, 0, json_integer((size_t)func_ptr)) == 0;
	}
	va_end(va);
	return ret;
}

int iat_detour_apply(HMODULE hMod)
{
	const char *dll_name;
	json_t *funcs;
	int ret = 0;

	if(!hMod) {
		return -1;
	}
	json_object_foreach(detours, dll_name, funcs) {
		HMODULE hDll = GetModuleHandleA(dll_name);
		size_t func_count = json_object_size(funcs);
		if(hDll && func_count) {
			const char *func_name;
			json_t *ptrs;
			VLA(iat_detour_t, iat_detour, func_count);
			size_t i = 0;
			json_object_foreach(funcs, func_name, ptrs) {
				iat_detour[i].old_func = func_name;
				iat_detour[i].new_ptr = (const void*)json_array_get_hex(ptrs, 0);
				iat_detour[i].old_ptr = GetProcAddress(hDll, iat_detour[i].old_func);
				i++;
			}
			ret += iat_detour_funcs(hMod, dll_name, iat_detour, func_count);
			VLA_FREE(iat_detour);
		}
	}
	return ret;
}

size_t detour_next(const char *dll_name, const char *func_name, void *caller, size_t arg_count, ...)
{
	json_t *funcs = json_object_get(detours, dll_name);
	json_t *ptrs = json_object_get(funcs, func_name);
	json_t *ptr = NULL;
	FARPROC next = NULL;
	size_t i;
	va_list va;

	// If anything is wrong with a detour_next() call, we're pretty much screwed,
	// as we can't know what to return to gracefully continue program execution.
	// So, just crashing outright with nice errors is the best thing we can do.
	if(!json_array_size(ptrs)) {
		log_printf(
			"["__FUNCTION__"]: No detours for %s:%s; typo?\n",
			dll_name, func_name
		);
	}
	// Get the next function after [caller]
	json_array_foreach(ptrs, i, ptr) {
		if((void*)json_integer_value(ptr) == caller) {
			next = (FARPROC)json_array_get_hex(ptrs, i + 1);
			break;
		}
	}
	if(!next) {
		HMODULE hDll = GetModuleHandleA(dll_name);
		next = GetProcAddress(hDll, func_name);
	}
	if(!next) {
		log_printf(
			"["__FUNCTION__"]: Couldn't get original function pointer for %s:%s! "
			"Time to crash...\n",
			dll_name, func_name
		);
	}
	// This might seem pointless, and we indeed could just set ESP to [va],
	// but Debug mode doesn't like that.
	va_rev_start(va, arg_count, arg_count);
	for(i = 0; i < arg_count; i++) {
		DWORD param = va_rev_arg(va, DWORD);
		__asm {
			push param
		}
	}
	// Same here.
	__asm {
		call next
	}
}

void detour_mod_exit()
{
	detours = json_decref_safe(detours);
}
/// ----------

/// =============================
