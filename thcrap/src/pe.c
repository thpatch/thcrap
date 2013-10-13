/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Parsing of Portable Executable structures.
  */

#include <thcrap.h>

// Adapted from http://forum.sysinternals.com/createprocess-api-hook_topic13138.html
PIMAGE_NT_HEADERS WINAPI GetNtHeader(HMODULE hMod)
{
	PIMAGE_DOS_HEADER pDosH;
	PIMAGE_NT_HEADERS pNTH;

	if(!hMod) {
		return 0;
	}
	// Get DOS Header
	pDosH = (PIMAGE_DOS_HEADER) hMod;

	// Verify that the PE is valid by checking e_magic's value and DOS Header size
	if(IsBadReadPtr(pDosH, sizeof(IMAGE_DOS_HEADER))) {
		return 0;
	}
	if(pDosH->e_magic != IMAGE_DOS_SIGNATURE) {
		return 0;
	}
	// Find the NT Header by using the offset of e_lfanew value from hMod
	pNTH = (PIMAGE_NT_HEADERS)((DWORD)pDosH + (DWORD)pDosH->e_lfanew);

	// Verify that the NT Header is correct
	if(IsBadReadPtr(pNTH, sizeof(IMAGE_NT_HEADERS))) {
		return 0;
	}
	if(pNTH->Signature != IMAGE_NT_SIGNATURE) {
		return 0;
	}
	return pNTH;
}

PIMAGE_IMPORT_DESCRIPTOR WINAPI GetDllImportDesc(HMODULE hMod, const char *DLLName)
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

PIMAGE_EXPORT_DIRECTORY WINAPI GetDllExportDesc(HMODULE hMod)
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

PIMAGE_SECTION_HEADER WINAPI GetSectionHeader(HMODULE hMod, const char *section_name)
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

	if(IsBadReadPtr(pSH, sizeof(IMAGE_SECTION_HEADER) * pNTH->FileHeader.NumberOfSections)) {
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

		log_printf("0x%08x %s\n", dll_base + func_ptrs[i], name);

		json_object_set_new(funcs, name, json_integer(dll_base + func_ptrs[i]));
	}
	return 0;
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
	//------

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
