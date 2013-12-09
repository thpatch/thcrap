/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Parsing of Portable Executable structures.
  */

#include "thcrap.h"

PIMAGE_NT_HEADERS GetNtHeader(HMODULE hMod)
{
	PIMAGE_DOS_HEADER pDosH;
	PIMAGE_NT_HEADERS pNTH;

	if(!hMod) {
		return 0;
	}
	// Get DOS Header
	pDosH = (PIMAGE_DOS_HEADER)hMod;

	if(
		!VirtualCheckRegion(pDosH, sizeof(IMAGE_DOS_HEADER))
		|| pDosH->e_magic != IMAGE_DOS_SIGNATURE
	) {
		return 0;
	}
	// Find the NT Header by using the offset of e_lfanew value from hMod
	pNTH = (PIMAGE_NT_HEADERS)((DWORD)pDosH + (DWORD)pDosH->e_lfanew);

	if(
		!VirtualCheckRegion(pNTH, sizeof(IMAGE_NT_HEADERS))
		|| pNTH->Signature != IMAGE_NT_SIGNATURE
	) {
		return 0;
	}
	return pNTH;
}

PIMAGE_IMPORT_DESCRIPTOR GetDllImportDesc(HMODULE hMod, const char *DLLName)
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
	pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD)hMod +
		(DWORD)(pNTH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress));

	if(pImportDesc == (PIMAGE_IMPORT_DESCRIPTOR)pNTH) {
		return NULL;
	}
	while(pImportDesc->Name) {
		char *name = (char*)((DWORD)hMod + (DWORD)pImportDesc->Name);
		if(stricmp(name, DLLName) == 0) {
			return pImportDesc;
		}
		++pImportDesc;
	}
	return NULL;
}

PIMAGE_EXPORT_DIRECTORY GetDllExportDesc(HMODULE hMod)
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

PIMAGE_SECTION_HEADER GetSectionHeader(HMODULE hMod, const char *section_name)
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

	if(!VirtualCheckRegion(pSH, sizeof(IMAGE_SECTION_HEADER) * pNTH->FileHeader.NumberOfSections)) {
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

int GetExportedFunctions(json_t *funcs, HMODULE hDll)
{
	IMAGE_EXPORT_DIRECTORY *ExportDesc;
	DWORD *func_ptrs = NULL;
	DWORD *name_ptrs = NULL;
	WORD *name_indices = NULL;
	DWORD dll_base = (DWORD)hDll; // All this type-casting is annoying
	WORD i, j; // can only ever be 16-bit values

	if(!funcs) {
		return -1;
	}

	ExportDesc = GetDllExportDesc(hDll);
	if(!ExportDesc) {
		return -2;
	}

	func_ptrs = (DWORD*)(ExportDesc->AddressOfFunctions + dll_base);
	name_ptrs = (DWORD*)(ExportDesc->AddressOfNames + dll_base);
	name_indices = (WORD*)(ExportDesc->AddressOfNameOrdinals + dll_base);

	for(i = 0; i < ExportDesc->NumberOfFunctions; i++) {
		DWORD name_ptr = 0;
		const char *name;
		char auto_name[16];

		// Look up name
		for(j = 0; (j < ExportDesc->NumberOfNames && !name_ptr); j++) {
			if(name_indices[j] == i) {
				name_ptr = name_ptrs[j];
			}
		}
		if(name_ptr) {
			name = (const char*)(dll_base + name_ptr);
		} else {
			itoa(i + ExportDesc->Base, auto_name, 10);
			name = auto_name;
		}
		json_object_set_new(funcs, name, json_integer(dll_base + func_ptrs[i]));
	}
	return 0;
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

		region_len = (size_t)mbi.BaseAddress + mbi.RegionSize - (size_t)addr;
		full_len += region_len;
		new_ret = realloc(ret, full_len);
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
		ret = (void*)(NTH.OptionalHeader.AddressOfEntryPoint + (DWORD)hMod);
	}
	return ret;
}

HMODULE GetRemoteModuleHandle(HANDLE hProcess, const char *search_module)
{
	HMODULE *modules = NULL;
	DWORD modules_size;
	HMODULE ret = NULL;
	STRLEN_DEC(search_module);

	if(!search_module) {
		return ret;
	}

	EnumProcessModules(hProcess, modules, 0, &modules_size);
	modules = (HMODULE*)malloc(modules_size);

	if(EnumProcessModules(hProcess, modules, modules_size, &modules_size)) {
		size_t i;
		size_t modules_num = modules_size / sizeof(HMODULE);
		for(i = 0; i < modules_num; i++) {
			char cur_module[MAX_PATH];
			if(GetModuleFileNameEx(hProcess, modules[i], cur_module, sizeof(cur_module))) {
				// Compare the end of the string to [search_module]. This makes the
				// function easily work with both fully qualified paths and bare file names.
				STRLEN_DEC(cur_module);
				int cmp_offset = cur_module_len - search_module_len;
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
	SAFE_FREE(modules);
	return ret;
}

FARPROC GetRemoteProcAddress(HANDLE hProcess, HMODULE hMod, LPCSTR lpProcName)
{
	FARPROC ret = NULL;
	BYTE *addr = (BYTE*)hMod;
	IMAGE_NT_HEADERS NTH;
	PIMAGE_DATA_DIRECTORY pExportPos;
	IMAGE_EXPORT_DIRECTORY ExportDesc;
	DWORD *func_ptrs = NULL;
	DWORD *name_ptrs = NULL;
	WORD *name_indices = NULL;
	DWORD i;
	DWORD ordinal = (DWORD)lpProcName;

	if(!lpProcName) {
		goto end;
	}
	if(GetRemoteModuleNtHeader(&NTH, hProcess, hMod)) {
		goto end;
	}
	if(NTH.OptionalHeader.NumberOfRvaAndSizes >= IMAGE_DIRECTORY_ENTRY_EXPORT + 1) {
		pExportPos = &NTH.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	} else {
		goto end;
	}

	if(ReadProcessMemory(hProcess, addr + pExportPos->VirtualAddress, &ExportDesc, sizeof(ExportDesc), NULL))
	{
		void *func_ptrs_pos = addr + ExportDesc.AddressOfFunctions;
		void *name_ptrs_pos = addr + ExportDesc.AddressOfNames;
		void *name_indices_pos = addr + ExportDesc.AddressOfNameOrdinals;
		size_t func_ptrs_size = ExportDesc.NumberOfFunctions * sizeof(DWORD);
		size_t name_ptrs_size = ExportDesc.NumberOfNames * sizeof(DWORD);
		size_t name_indices_size = ExportDesc.NumberOfNames * sizeof(WORD);

		func_ptrs = malloc(func_ptrs_size);
		name_ptrs = malloc(name_ptrs_size);
		name_indices = malloc(name_indices_size);
		if(
			!func_ptrs || !name_ptrs || !name_indices
			|| !ReadProcessMemory(hProcess, func_ptrs_pos, func_ptrs, func_ptrs_size, NULL)
			|| !ReadProcessMemory(hProcess, name_ptrs_pos, name_ptrs, name_ptrs_size, NULL)
			|| !ReadProcessMemory(hProcess, name_indices_pos, name_indices, name_indices_size, NULL)
		) {
			goto end;
		}
	} else {
		goto end;
	}

	for(i = 0; i < ExportDesc.NumberOfFunctions && !ret; i++) {
		DWORD name_ptr = 0;
		WORD j;
		for(j = 0; j < ExportDesc.NumberOfNames && !name_ptr; j++) {
			if(name_indices[j] == i) {
				name_ptr = name_ptrs[j];
			}
		}
		if(name_ptr) {
			char *name = ReadProcessString(hProcess, addr + name_ptr);
			if(name && !strcmp(name, lpProcName)) {
				ret = (FARPROC)(addr + func_ptrs[i]);
			}
			SAFE_FREE(name);
		} else if(HIWORD(ordinal) == 0 && LOWORD(ordinal) == i + ExportDesc.Base) {
			ret = (FARPROC)(addr + func_ptrs[i]);
		}
	}
end:
	SAFE_FREE(func_ptrs);
	SAFE_FREE(name_ptrs);
	SAFE_FREE(name_indices);
	return ret;
}
