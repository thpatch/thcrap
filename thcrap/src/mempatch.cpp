/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Direct memory patching and Import Address Table detouring.
  */

#include "thcrap.h"

struct detour_func_map_t {
	std::unordered_map<std::string_view, UINT_PTR> funcs;
	bool disable = false;
};

static std::unordered_map<std::string_view, detour_func_map_t> detours;

BOOL VirtualCheckRegion(const void *ptr, const size_t len)
{
	MEMORY_BASIC_INFORMATION mbi;
	if (VirtualQuery(ptr, &mbi, sizeof(MEMORY_BASIC_INFORMATION)) == 0) {
		return FALSE;
	}

	auto ptr_end = (size_t)ptr + len;
	auto page_end = (size_t)mbi.BaseAddress + mbi.RegionSize;
	if (page_end < (ptr_end)) {
		return FALSE;
	}

	if (mbi.Protect == 0 || (mbi.Protect & PAGE_NOACCESS)) {
		return FALSE;
	}

	return TRUE;
}

BOOL VirtualCheckCode(const void *ptr)
{
	return VirtualCheckRegion(ptr, 1);
}

int PatchRegion(void *ptr, const void *Prev, const void *New, size_t len)
{
	int ret = 0;

	if(VirtualCheckRegion(ptr, len)) {
		DWORD oldProt;
		VirtualProtect(ptr, len, PAGE_READWRITE, &oldProt);
		if (!Prev || memcmp(ptr, Prev, len) == 0) {
			memcpy(ptr, New, len);
			ret = 1;
		}
		VirtualProtect(ptr, len, oldProt, &oldProt);
	}
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

void* PatchRegionCopySrc(void *ptr, const void *Prev, const void *New, void *CpyBuf, size_t len)
{
	void* ret = NULL;

	if(VirtualCheckRegion(ptr, len)) {
		DWORD oldProt;
		VirtualProtect(ptr, len, PAGE_READWRITE, &oldProt);
		if (!Prev || memcmp(ptr, Prev, len) == 0) {
			memcpy(CpyBuf, ptr, len);
			memcpy(ptr, New, len);
			ret = CpyBuf;
		}
		VirtualProtect(ptr, len, oldProt, &oldProt);
	}
	return ret;
}

/// Import Address Table detouring
/// ==============================
inline int func_detour(PIMAGE_THUNK_DATA pThunk, const void *new_ptr)
{
	return PatchRegion(&pThunk->u1.Function, NULL, &new_ptr, sizeof(new_ptr));
}

int iat_detour_func(HMODULE hMod, PIMAGE_IMPORT_DESCRIPTOR pImpDesc, const iat_detour_t *detour)
{
	if(!hMod || !pImpDesc || !detour || !detour->new_ptr || !detour->old_func) {
		return -1;
	}
	if(!VirtualCheckCode(detour->new_ptr)) {
		return -2;
	}
	auto pOT = (PIMAGE_THUNK_DATA)((UINT_PTR)hMod + pImpDesc->OriginalFirstThunk);
	auto pIT = (PIMAGE_THUNK_DATA)((UINT_PTR)hMod + pImpDesc->FirstThunk);

	// We generally detour by comparing exported names. This has the advantage
	// that we can override any existing patches, and that it works on Win9x
	// too (as if that matters). However, in case we lack a pointer to the
	// OriginalFirstThunk, or the function is only exported by ordinal, this
	// is not possible, so we have to detour by comparing pointers then.

	if(pImpDesc->OriginalFirstThunk) {
		for(; pOT->u1.Function; pOT++, pIT++) {
			PIMAGE_IMPORT_BY_NAME pByName;
			if(!(pOT->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
				pByName = (PIMAGE_IMPORT_BY_NAME)((UINT_PTR)hMod + pOT->u1.AddressOfData);
				if(pByName->Name[0] == '\0') {
					return 0;
				}
				if(!stricmp(detour->old_func, (char*)pByName->Name)) {
					return func_detour(pIT, detour->new_ptr);
				}
			} else {
				if((void*)pIT->u1.Function == detour->old_ptr) {
					return func_detour(pIT, detour->new_ptr);
				}
			}
		}
	} else {
		for(; pIT->u1.Function; pIT++) {
			if((void*)pIT->u1.Function == detour->old_ptr) {
				return func_detour(pIT, detour->new_ptr);
			}
		}
	}
	// Function not found
	return 0;
}
/// ----------

/// Detour chaining
/// ---------------
detour_func_map_t& detour_get_create(const char *dll_name)
{
	const char* dll_name_lower = _strlwr(strdup(dll_name));
	auto ret = detours.try_emplace(dll_name_lower);
	if (!ret.second) {
		free((void*)dll_name_lower);
	}
	return ret.first->second;
}

int detour_chain(const char *dll_name, int return_old_ptrs, ...)
{
	int ret = 0;
	detour_func_map_t& dll = detour_get_create(dll_name);
	if (!dll.disable) {
		const char* func_name = NULL;
		va_list va;
		va_start(va, return_old_ptrs);
		while ((func_name = va_arg(va, const char*))) {

			const void* func_ptr = va_arg(va, const void*);

			FARPROC* old_ptr = return_old_ptrs ? va_arg(va, FARPROC*) : NULL;

			if (dll.funcs.count(func_name) && !dll.funcs[func_name]) {
				continue;
			}

			if (FARPROC chain_ptr;
				old_ptr
				&& (chain_ptr = (FARPROC)dll.funcs[func_name])
				&& (chain_ptr != func_ptr)
				) {
				*old_ptr = chain_ptr;
			}

			dll.funcs[func_name] = (UINT_PTR)func_ptr;
		}
		va_end(va);
	}
	return ret;
}

int detour_chain_w32u8(const w32u8_dll_t *dll)
{
	const w32u8_pair_t *pair = NULL;

	if(!dll || !dll->name || !dll->funcs) {
		return -1;
	}
	detour_func_map_t& detours_dll = detour_get_create(dll->name);
	pair = dll->funcs;
	while(pair && pair->ansi_name && pair->utf8_ptr) {
		if (const char* func_name = strdup(pair->ansi_name);
			!detours_dll.funcs.try_emplace(func_name, (UINT_PTR)pair->utf8_ptr).second)
		{
			free((void*)func_name);
		}
		pair++;
	}
	return 0;
}

void detour_disable(const char* dll_name, const char* func_name)
{
	detour_func_map_t& dll = detour_get_create(dll_name);
	if (func_name) {
		dll.funcs[func_name] = NULL;
	}
	else {
		dll.disable = true;
	}
	
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
		char* dll_name = strdup((char*)((UINT_PTR)hMod + (UINT_PTR)pImpDesc->Name));
		HMODULE hDll = GetModuleHandleA(dll_name);
		_strlwr(dll_name);
		detour_func_map_t& dll = detours[dll_name];
		if (!dll.disable) {

			size_t func_count = dll.funcs.size();

			if (hDll && func_count) {
				size_t i = 0;

				log_printf("Detouring DLL functions (%s)...\n", dll_name);

				for (auto ptr : dll.funcs) {
					++i;
					if (ptr.second == NULL) {
						continue;
					}
					iat_detour_t detour;
					int local_ret;

					detour.old_func = ptr.first.data();
					detour.old_ptr = (void*)GetProcAddress(hDll, detour.old_func);
					detour.new_ptr = (void*)ptr.second;

					local_ret = iat_detour_func(hMod, pImpDesc, &detour);

					log_printf(
						"(%2d/%2d) %s... %s\n",
						i, func_count, detour.old_func,
						local_ret ? "OK" : "not found"
					);
				}
			}
			free(dll_name);
		}
		pImpDesc++;
	}
	return ret;
}

FARPROC detour_top(const char *dll_name, const char *func_name, FARPROC fallback)
{
	FARPROC ret = fallback;
	if (detours.count(dll_name)) {
		detour_func_map_t& dll = detours[dll_name];
		if (dll.funcs.count(func_name)) {
			ret = (FARPROC)dll.funcs[func_name];
		}
	}
	return ret;
}

int vtable_detour(void **vtable, const vtable_detour_t *det, size_t det_count)
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
		lowest = MIN(lowest, det[i].index);
		highest = MAX(highest, det[i].index);
	}

	size_t bytes_to_lock = ((highest + 1) - lowest) * sizeof(void*);

	VirtualProtect(vtable + lowest, bytes_to_lock, PAGE_READWRITE, &old_prot);
	for(i = 0; i < det_count; i++) {
		auto& cur = det[i];

		bool replace =
			(cur.old_func == nullptr) || (*cur.old_func == nullptr)
			|| (*cur.old_func == vtable[cur.index]);
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
/// ----------

#if TH_X64
/// x64 manipulation of virtual memory
/// that can be accessed with absolute encodings
/// ----------
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
		return FALSE;
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
/// ----------
#endif

/// =============================
