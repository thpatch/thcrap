/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Parsing of Portable Executable structures.
  */

#include "thcrap.h"

#define EXPORT_FOUND 0
#define EXPORT_NOT_FOUND 1
#define INVALID_FILE 2

int find_export_in_headers(void* buffer, const char* const func_name) {
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)buffer;
	if unexpected(pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
		return INVALID_FILE;
	}
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((UINT_PTR)buffer + pDosHeader->e_lfanew);
	if unexpected(pNtHeader->Signature != IMAGE_NT_SIGNATURE) {
		return INVALID_FILE;
	}
#if TH_X86
	if unexpected(pNtHeader->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
		return INVALID_FILE;
	}
#else
	if unexpected(pNtHeader->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) {
		return INVALID_FILE;
	}
#endif
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNtHeader);

	if (pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress != 0 && pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size != 0) {
		DWORD pExportSectionVA = pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
		for (size_t i = 0; i < pNtHeader->FileHeader.NumberOfSections; i++, pSection++) {
			if (pSection->VirtualAddress <= pExportSectionVA && pSection->VirtualAddress + pSection->SizeOfRawData > pExportSectionVA) {
				uintptr_t pSectionBase = (uintptr_t)buffer - pSection->VirtualAddress + pSection->PointerToRawData;
				PIMAGE_EXPORT_DIRECTORY pExportDirectory = (PIMAGE_EXPORT_DIRECTORY)(pSectionBase + pExportSectionVA);
				DWORD* pExportNames = (DWORD*)(pSectionBase + pExportDirectory->AddressOfNames);
				for (size_t j = 0; j < pExportDirectory->NumberOfNames; ++j) {
					char* pFunctionName = (char*)(pSectionBase + pExportNames[j]);
					if (!strcmp(pFunctionName, func_name)) {
						return EXPORT_FOUND;
					}
				}
			}
		}
	}

	return EXPORT_NOT_FOUND;
}

PluginValidation validate_plugin_dll_for_load(const char* const path) {
	HMODULE dll = LoadLibraryExU(path, NULL, DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE | LOAD_WITH_ALTERED_SEARCH_PATH);
	if unexpected(!dll) {
		// Loading completely failed
		return NOT_A_DLL;
	}
	if (LDR_IS_VIEW(dll)) {
		// Module is already loaded, so loading it
		// just increased the reference count.
		// Call FreeLibrary to put it back.
		FreeLibrary(dll);
		return ALREADY_LOADED;
	}

	PluginValidation ret = (PluginValidation)find_export_in_headers(LDR_DATAFILE_TO_VIEW(dll), "thcrap_plugin_init");
	FreeLibrary(dll);
	return ret;
}

bool CheckDLLFunction(const char* const path, const char* const func_name)
{
	HANDLE hFile = CreateFileU(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;
	defer(CloseHandle(hFile));
	DWORD fileSize = GetFileSize(hFile, NULL);
	if (fileSize > (1 << 23) || fileSize < 128)
		return false;
	HANDLE hFileMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, fileSize, NULL);
	if (!hFileMap)
		return false;
	defer(CloseHandle(hFileMap));
	void* pFileMapView = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, fileSize);
	if (!pFileMapView)
		return false;
	defer(UnmapViewOfFile(pFileMapView));

	return find_export_in_headers(pFileMapView, func_name) == EXPORT_FOUND;
}

PIMAGE_NT_HEADERS GetNtHeader(HMODULE hMod)
{
	if unexpected(!hMod) {
		return 0;
	}

	// Get DOS Header
	PIMAGE_DOS_HEADER pDosH = (PIMAGE_DOS_HEADER)hMod;

	if(
		!VirtualCheckRegion(pDosH, sizeof(IMAGE_DOS_HEADER))
		|| pDosH->e_magic != IMAGE_DOS_SIGNATURE
	) {
		return 0;
	}
	// Find the NT Header by using the offset of e_lfanew from hMod
	PIMAGE_NT_HEADERS pNTH = (PIMAGE_NT_HEADERS)((UINT_PTR)pDosH + pDosH->e_lfanew);

	if(
		!VirtualCheckRegion(pNTH, sizeof(IMAGE_NT_HEADERS))
		|| pNTH->Signature != IMAGE_NT_SIGNATURE
	) {
		return 0;
	}
	return pNTH;
}

void *GetNtDataDirectory(HMODULE hMod, BYTE directory)
{
	assert(directory <= IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR);
	if(PIMAGE_NT_HEADERS pNTH = GetNtHeader(hMod)) {
		if(UINT_PTR DirVA = pNTH->OptionalHeader.DataDirectory[directory].VirtualAddress) {
			return (BYTE*)hMod + DirVA;
		}
	}
	return NULL;
}

PIMAGE_IMPORT_DESCRIPTOR GetDllImportDesc(HMODULE hMod, const char *dll_name)
{
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)GetNtDataDirectory(hMod, IMAGE_DIRECTORY_ENTRY_IMPORT);
	if unexpected(!pImportDesc) {
		return NULL;
	}
	while(pImportDesc->Name) {
		char *name = (char*)((UINT_PTR)hMod + pImportDesc->Name);
		if(stricmp(name, dll_name) == 0) {
			return pImportDesc;
		}
		++pImportDesc;
	}
	return NULL;
}

PIMAGE_EXPORT_DIRECTORY GetDllExportDesc(HMODULE hMod)
{
	return (PIMAGE_EXPORT_DIRECTORY)GetNtDataDirectory(hMod, IMAGE_DIRECTORY_ENTRY_EXPORT);
}

PIMAGE_SECTION_HEADER GetSectionHeader(HMODULE hMod, const char *section_name)
{
	if unexpected(!hMod || !section_name) {
		return 0;
	}
	PIMAGE_NT_HEADERS pNTH = GetNtHeader(hMod);
	if(!pNTH) {
		return NULL;
	}
	// OptionalHeader position + SizeOfOptionalHeader = Section headers
	PIMAGE_SECTION_HEADER pSH = (PIMAGE_SECTION_HEADER)((UINT_PTR)&pNTH->OptionalHeader + pNTH->FileHeader.SizeOfOptionalHeader);

	if(!VirtualCheckRegion(pSH, sizeof(IMAGE_SECTION_HEADER) * pNTH->FileHeader.NumberOfSections)) {
		return 0;
	}
	// Search
	for(size_t c = 0; c < pNTH->FileHeader.NumberOfSections; c++) {
		if(strncmp((const char*)pSH->Name, section_name, 8) == 0) {
			return pSH;
		}
		++pSH;
	}
	return NULL;
}

exported_func_t* GetExportedFunctions(HMODULE hDll)
{
	IMAGE_EXPORT_DIRECTORY* ExportDesc = GetDllExportDesc(hDll);
	if(!ExportDesc) {
		return NULL;
	}

	UINT_PTR dll_base = (UINT_PTR)hDll; // All this type-casting is annoying
	DWORD* func_ptrs = (DWORD*)(ExportDesc->AddressOfFunctions + dll_base);
	DWORD* name_ptrs = (DWORD*)(ExportDesc->AddressOfNames + dll_base);
	WORD* name_indices = (WORD*)(ExportDesc->AddressOfNameOrdinals + dll_base);

	exported_func_t* funcs = (exported_func_t*)malloc((ExportDesc->NumberOfFunctions + 1) * sizeof(exported_func_t));
	funcs[ExportDesc->NumberOfFunctions] = {0, 0};

	// i and j can only ever be 16-bit values
	char auto_name[_MAX_ITOSTR_BASE10_COUNT];
	for(size_t i = 0; i < ExportDesc->NumberOfFunctions; i++) {
		const char *name = NULL;

		// Look up name
		for(size_t j = 0; j < ExportDesc->NumberOfNames; j++) {
			if(name_indices[j] == i) {
				name = (const char*)(dll_base + name_ptrs[j]);
				break;
			}
		}
		if(!name) {
			itoa((int)i + ExportDesc->Base, auto_name, 10);
			name = auto_name;
		}
		funcs[i].name = name;
		funcs[i].func = dll_base + func_ptrs[i];
	}
	return funcs;
}

HMODULE GetModuleContaining(void *addr)
{
	HMODULE ret = nullptr;
	if(!GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
		(LPTSTR)addr,
		&ret
	)) {
		// Just to be sure...
		return nullptr;
	}
	return ret;
}

char* ReadProcessString(HANDLE hProcess, LPCVOID lpBaseAddress)
{
	MEMORY_BASIC_INFORMATION mbi;
	size_t pos = 0;
	size_t full_len = 0;
	size_t region_len = 0;
	char *ret = NULL;
	char *addr = (char*)lpBaseAddress;

	do {
		char *new_ret;
		pos += region_len;
		addr += region_len;

		if(!VirtualQueryEx(hProcess, lpBaseAddress, &mbi, sizeof(mbi))) {
			break;
		}

		region_len = (uintptr_t)mbi.BaseAddress + mbi.RegionSize - (uintptr_t)addr;
		full_len += region_len;
		new_ret = (char *)realloc(ret, full_len);
		if(new_ret) {
			ret = new_ret;
		} else {
			break;
		}
		if(!ReadProcessMemory(hProcess, addr, ret + pos, region_len, NULL)) {
			if(!pos) {
				SAFE_FREE(ret);
			}
			break;
		}
	} while(!memchr(ret + pos, 0, region_len));
	// Make sure the string is null-terminated
	// (might not be the case if we broke out of the loop)
	if(ret) {
		ret[full_len - 1] = 0;
	}
	return ret;
}

int GetRemoteModuleNtHeader(PIMAGE_NT_HEADERS pNTH, HANDLE hProcess, HMODULE hMod)
{
	BYTE *addr = (BYTE*)hMod;
	IMAGE_DOS_HEADER DosH;

	if(!pNTH) {
		return -1;
	}

	ReadProcessMemory(hProcess, addr, &DosH, sizeof(DosH), NULL);
	if(DosH.e_magic != IMAGE_DOS_SIGNATURE) {
		return 2;
	}
	addr += DosH.e_lfanew;
	ReadProcessMemory(hProcess, addr, pNTH, sizeof(IMAGE_NT_HEADERS), NULL);

	return pNTH->Signature != IMAGE_NT_SIGNATURE;
}

void* GetRemoteModuleEntryPoint(HANDLE hProcess, HMODULE hMod)
{
	// No, GetModuleInformation() would not be an equivalent shortcut.
	void *ret = NULL;
	IMAGE_NT_HEADERS NTH;
	if(!GetRemoteModuleNtHeader(&NTH, hProcess, hMod)) {
		ret = (void*)(NTH.OptionalHeader.AddressOfEntryPoint + (UINT_PTR)hMod);
	}
	return ret;
}

HMODULE GetRemoteModuleHandle(HANDLE hProcess, const char *search_module)
{
	if unexpected(!search_module) {
		return NULL;
	}

	DWORD modules_size;
	EnumProcessModules(hProcess, NULL, 0, &modules_size);
	HMODULE* modules = (HMODULE*)malloc(modules_size);


	HMODULE ret = NULL;
	if(EnumProcessModules(hProcess, modules, modules_size, &modules_size)) {
		size_t search_module_len = strlen(search_module);

		size_t modules_num = modules_size / sizeof(HMODULE);

		char cur_module[MAX_PATH];
		for(size_t i = 0; i < modules_num; i++) {
			if(GetModuleFileNameEx(hProcess, modules[i], cur_module, sizeof(cur_module))) {
				// Compare the end of the string to [search_module]. This makes the
				// function easily work with both fully qualified paths and bare file names.
				size_t cur_module_len = strlen(cur_module);
				intptr_t cmp_offset = cur_module_len - search_module_len;
				if(
					cmp_offset >= 0
					&& !strnicmp(search_module, cur_module + cmp_offset, cur_module_len)
				) {
					ret = modules[i];
					break;
				}
			}
		}
	}

	free(modules);
	return ret;
}

FARPROC GetRemoteProcAddress(HANDLE hProcess, HMODULE hMod, LPCSTR lpProcName)
{
	BYTE *addr = (BYTE*)hMod;

	if unexpected(!lpProcName) {
		return NULL;
	}

	IMAGE_NT_HEADERS NTH;
	if unexpected(GetRemoteModuleNtHeader(&NTH, hProcess, hMod)) {
		return NULL;
	}
	if unexpected(NTH.OptionalHeader.NumberOfRvaAndSizes < IMAGE_DIRECTORY_ENTRY_EXPORT + 1) {
		return NULL;
	}

	PIMAGE_DATA_DIRECTORY pExportPos = &NTH.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

	IMAGE_EXPORT_DIRECTORY ExportDesc;
	if unexpected(!ReadProcessMemory(hProcess, addr + pExportPos->VirtualAddress, &ExportDesc, sizeof(ExportDesc), NULL)) {
		return NULL;
	}

	void *func_ptrs_pos = addr + ExportDesc.AddressOfFunctions;
	void *name_ptrs_pos = addr + ExportDesc.AddressOfNames;
	void *name_indices_pos = addr + ExportDesc.AddressOfNameOrdinals;
	size_t func_ptrs_size = ExportDesc.NumberOfFunctions * sizeof(DWORD);
	size_t name_ptrs_size = ExportDesc.NumberOfNames * sizeof(DWORD);
	size_t name_indices_size = ExportDesc.NumberOfNames * sizeof(WORD);

	uint8_t* buffer = (uint8_t*)malloc(func_ptrs_size + name_ptrs_size + name_indices_size);
	if unexpected(!buffer) {
		return NULL;
	}

	FARPROC ret = NULL;
	DWORD ordinal = (DWORD)lpProcName;

	DWORD* func_ptrs = (DWORD*)buffer;
	DWORD* name_ptrs = (DWORD*)&buffer[func_ptrs_size];
	WORD* name_indices = (WORD*)&buffer[func_ptrs_size + name_ptrs_size];
	if (
		ReadProcessMemory(hProcess, func_ptrs_pos, func_ptrs, func_ptrs_size, NULL) &&
		ReadProcessMemory(hProcess, name_ptrs_pos, name_ptrs, name_ptrs_size, NULL) &&
		ReadProcessMemory(hProcess, name_indices_pos, name_indices, name_indices_size, NULL)
	) {
		for (size_t i = 0; i < ExportDesc.NumberOfFunctions && !ret; i++) {
			size_t name_ptr = 0;
			for(size_t j = 0; j < ExportDesc.NumberOfNames; j++) {
				if (name_indices[j] == i) {
					name_ptr = name_ptrs[j];
					break;
				}
			}
			if(name_ptr) {
				if (char *name = ReadProcessString(hProcess, addr + name_ptr)) {
					if (!strcmp(name, lpProcName)) {
						ret = (FARPROC)(addr + func_ptrs[i]);
					}
					free(name);
				}
			} else if(HIWORD(ordinal) == 0 && LOWORD(ordinal) == i + ExportDesc.Base) {
				ret = (FARPROC)(addr + func_ptrs[i]);
			}
		}
	}

	free(buffer);
	return ret;
}
