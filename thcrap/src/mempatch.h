/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Direct memory and Import Address Table patching
  */

#pragma once

// Writes [val] to [ptr] in the address space of the current
// (or another, in case of the *Ex functions) process),
// if the current value in [ptr] equals [prev]
int PatchRegionNoCheck(void *ptr, void *New, size_t len);
int PatchRegion(void *ptr, void *Prev, void *New, size_t len);
int PatchRegionEx(HANDLE hProcess, void *ptr, void *Prev, void *New, size_t len);
int PatchBYTE(void *ptr, BYTE Prev, BYTE val);
int PatchDWORD(void *ptr, DWORD Prev, DWORD val);
int PatchFLOAT(void *ptr, FLOAT Prev, FLOAT val);
int PatchBYTEEx(HANDLE hProcess, void *ptr, BYTE Prev, BYTE val);
int PatchDWORDEx(HANDLE hProcess, void *ptr, DWORD Prev, DWORD val);
int PatchFLOATEx(HANDLE hProcess, void *ptr, FLOAT Prev, FLOAT val);

/// PE structures
/// -------------
// Returns the IMAGE_NT_HEADERS structure of [hMod]
PIMAGE_NT_HEADERS WINAPI GetNtHeader(HMODULE hMod);

// Returns the import descriptor of [DLLName]
PIMAGE_IMPORT_DESCRIPTOR WINAPI GetDllImportDesc(HMODULE hMod, const char *dll_name);

// Returns the export descriptor of the DLL with the base address [hMod]
PIMAGE_EXPORT_DIRECTORY WINAPI GetDllExportDesc(HMODULE hMod);

// Returns the section header named [section_name]
PIMAGE_SECTION_HEADER WINAPI GetSectionHeader(HMODULE hMod, const char *section_name);
/// -------------

/// DLL function macros
/// -------------------
// For external DLL functions, the form [(dll)_(func)] is used for the individual function pointers.

#define DLL_FUNC(dll, func) \
	dll##_##func
#define DLL_FUNC_TYPE(dll, func) \
	DLL_FUNC(dll, func)##_type

#define DLL_FUNC_DEC(dll, func) \
	extern DLL_FUNC_TYPE(dll, func) DLL_FUNC(dll, func)

#define DLL_FUNC_DEF(dll, func) \
	DLL_FUNC_TYPE(dll, func) DLL_FUNC(dll, func) = NULL

#define DLL_GET_PROC_ADDRESS(handle, dll, func) \
	DLL_FUNC(dll, func) = (DLL_FUNC_TYPE(dll, func))GetProcAddress(handle, #func)

#define DLL_GET_PROC_ADDRESS_REPORT(handle, dll, func) \
	DLL_GET_PROC_ADDRESS(handle, dll, func) \
	if(!DLL_FUNC(dll, func)) { \
		log_mboxf(NULL, MB_ICONEXCLAMATION | MB_OK, \
			"Function <%s> not found!", name); \
	}

#define DLL_SET_IAT_PATCH(num, dll, old_func, new_func) \
	iat_patch_set(&patch[num], #old_func, DLL_FUNC(dll, old_func), new_func)
/// -------------------

/// Import Address Table patching
/// =============================

/// Low-level
/// ---------
// Replaces the function pointer of [pThunk] with [new_ptr]
int func_patch(PIMAGE_THUNK_DATA pThunk, void *new_ptr);

// Searches for [old_func] starting from [pOrigFirstThunk]
// then patches the function with [new_ptr].
int func_patch_by_name(HMODULE hMod, PIMAGE_THUNK_DATA pOrigFirstThunk, PIMAGE_THUNK_DATA pImpFirstThunk, const char *old_func, void *new_ptr);

// Searches for [old_ptr] starting from [pImpFirstThunk],
// then patches the function with [new_ptr].
int func_patch_by_ptr(PIMAGE_THUNK_DATA pImpFirstThunk, void *old_ptr, void *new_ptr);
/// ---------

/// High-level
/// ----------
// Information about a single function to patch
typedef struct {
	const char *old_func;
	const void *old_ptr;
	const void *new_ptr;
} iat_patch_t;

// Convenience function to set a single iat_patch_t entry
void iat_patch_set(iat_patch_t* patch, const char *old_func, const void *old_ptr, const void *new_ptr);

// Patches [patch_count] functions in the [iat_patch] array
int iat_patch_funcs(HMODULE hMod, const char *dll_name, iat_patch_t *iat_patch, const size_t patch_count);

/**
  * Variadic wrapper around iat_patch_funcs().
  * Patches a number of functions imported from [dll_name] in the module based at [hMod].
  *
  * Expects [patch_count] * 2 additional parameters of the form
  *
  *	"exported name", new_func_ptr,
  *	"exported name", new_func_ptr,
  * ...
  */
int iat_patch_funcs_var(HMODULE hMod, const char *dll_name, const size_t patch_count, ...);
/// ----------

/// =============================
