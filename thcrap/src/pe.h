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
PIMAGE_NT_HEADERS WINAPI GetNtHeader(HMODULE hMod);

// Returns the import descriptor of [DLLName]
PIMAGE_IMPORT_DESCRIPTOR WINAPI GetDllImportDesc(HMODULE hMod, const char *dll_name);

// Returns the export descriptor of the DLL with the base address [hMod]
PIMAGE_EXPORT_DIRECTORY WINAPI GetDllExportDesc(HMODULE hMod);

// Returns the section header named [section_name]
PIMAGE_SECTION_HEADER WINAPI GetSectionHeader(HMODULE hMod, const char *section_name);

// Fills [funcs] with the names and function pointers of all exported functions of the DLL at [hDll].
int GetExportedFunctions(json_t *funcs, HMODULE hDll);
/// -----

/// Remote
/// ------
// Reads the entry point from the PE header.
void* entry_from_header(HANDLE hProcess, void *base_addr);

// Returns the base address of the module with the given title in [hProcess].
// [search_module] can be both a fully qualified path or a DLL/EXE file name.
void* module_base_get(HANDLE hProcess, const char *search_module);
/// ------
