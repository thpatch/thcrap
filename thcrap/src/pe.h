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

// Returns the import descriptor of [DLLName]
PIMAGE_IMPORT_DESCRIPTOR GetDllImportDesc(HMODULE hMod, const char *dll_name);

// Returns the export descriptor of the DLL with the base address [hMod]
PIMAGE_EXPORT_DIRECTORY GetDllExportDesc(HMODULE hMod);

// Returns the section header named [section_name]
PIMAGE_SECTION_HEADER GetSectionHeader(HMODULE hMod, const char *section_name);

// Fills [funcs] with the names and function pointers of all exported functions of the DLL at [hDll].
int GetExportedFunctions(json_t *funcs, HMODULE hDll);
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

// Reads a null-terminated string from [hProcess], beginning at [lpBaseAddress].
// Correctly handles strings crossing memory page boundaries.
// Return value has to be free()d by the caller!
char* ReadProcessString(HANDLE hProcess, LPCVOID lpBaseAddress);
/// ------
