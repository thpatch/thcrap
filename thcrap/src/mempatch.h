/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Direct memory and Import Address Table patching
  */

#pragma once

// IsBadReadPointer without the flawed implementation.
// Returns TRUE if [ptr] points to at least [len] bytes of valid memory in the
// address space of the current process.
BOOL VirtualCheckRegion(const void *ptr, const size_t len);
BOOL VirtualCheckCode(const void *ptr);

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

#define DLL_SET_IAT_DETOUR(num, dll, old_func, new_func) \
	iat_detour_set(&patch[num], #old_func, DLL_FUNC(dll, old_func), new_func)
/// -------------------

/// Import Address Table patching
/// =============================

// Information about a single function to detour
typedef struct {
	const char *old_func;
	const void *old_ptr;
	const void *new_ptr;
} iat_detour_t;

/// Low-level
/// ---------
// Replaces the function pointer of [pThunk] with [new_ptr]
int func_detour(PIMAGE_THUNK_DATA pThunk, const void *new_ptr);

// Sets up [detour] by name or pointer.
// Returns 1 if the function was found and detoured, 0 if it wasn't.
int func_detour_by_name(HMODULE hMod, PIMAGE_THUNK_DATA pOrigFirstThunk, PIMAGE_THUNK_DATA pImpFirstThunk, const iat_detour_t *detour);
int func_detour_by_ptr(PIMAGE_THUNK_DATA pImpFirstThunk, const iat_detour_t *detour);
/// ---------

/// High-level
/// ----------
// Convenience function to set a single iat_detour_t entry
void iat_detour_set(iat_detour_t* detour, const char *old_func, const void *old_ptr, const void *new_ptr);

// Detours [detour_count] functions in the [iat_detour] array
int iat_detour_funcs(HMODULE hMod, const char *dll_name, iat_detour_t *iat_detour, const size_t detour_count);

/**
  * Variadic wrapper around iat_detour_funcs().
  * Detours a number of functions imported from [dll_name] in the module based at [hMod].
  *
  * Expects [detour_count] * 2 additional parameters of the form
  *
  *	"exported name", new_func_ptr,
  *	"exported name", new_func_ptr,
  * ...
  */
int iat_detour_funcs_var(HMODULE hMod, const char *dll_name, const size_t detour_count, ...);
/// ----------

/// =============================
