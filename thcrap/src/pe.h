/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Parsing of Portable Executable structures.
  */

#pragma once

/// Local
/// -----
// Returns the IMAGE_NT_HEADERS structure of [hMod]
PIMAGE_NT_HEADERS GetNtHeader(HMODULE hMod);

// Returns a pointer to the given data directory in the NT header of [hMod],
// or NULL if [hMod] does not have an entry for that directory.
void *GetNtDataDirectory(HMODULE hMod, BYTE directory);

// Returns the import descriptor of [DLLName]
PIMAGE_IMPORT_DESCRIPTOR GetDllImportDesc(HMODULE hMod, const char *dll_name);

// Returns the export descriptor of the DLL with the base address [hMod]
PIMAGE_EXPORT_DIRECTORY GetDllExportDesc(HMODULE hMod);

// Returns the section header named [section_name]
PIMAGE_SECTION_HEADER GetSectionHeader(HMODULE hMod, const char *section_name);

// Adds the names and function pointers of all exported functions in the DLL
// at [hDll] to the JSON object [funcs].
int GetExportedFunctions(json_t *funcs, HMODULE hDll);

// Shorthand for GetModuleHandleEx() with GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS.
// Returns a nullptr on failure.
HMODULE GetModuleContaining(void *addr);
/// -----

/// Remote
/// ------
// Fills [pNTH] with the IMAGE_NT_HEADERS structure of [hMod] in [hProcess].
int GetRemoteModuleNtHeader(PIMAGE_NT_HEADERS pNTH, HANDLE hProcess, HMODULE hMod);

// Reads the entry point from the PE header.
void* GetRemoteModuleEntryPoint(HANDLE hProcess, HMODULE hMod);

// Returns the base address of the module with the given title in [hProcess].
// [search_module] can be both a fully qualified path or a DLL/EXE file name.
HMODULE GetRemoteModuleHandle(HANDLE hProcess, const char *search_module);

// GetProcAddress() for remote processes. Works with both names and ordinals,
// just like the original function.
FARPROC GetRemoteProcAddress(HANDLE hProcess, HMODULE hMod, LPCSTR lpProcName);

// Reads a null-terminated string from [hProcess], beginning at [lpBaseAddress].
// Correctly handles strings crossing memory page boundaries.
// Return value has to be free()d by the caller!
char* ReadProcessString(HANDLE hProcess, LPCVOID lpBaseAddress);
/// ------
