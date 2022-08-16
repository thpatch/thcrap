/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Parsing of Portable Executable structures.
  */

#include "thcrap.h"

bool CheckDLLFunction(const char* const path, const char* const func_name)
{
#define MakePointer(t, p, offset) ((t)((PUINT8)(p) + offset))
	HANDLE hFile = INVALID_HANDLE_VALUE;
	DWORD fileSize = 0;
	HANDLE hFileMap = NULL;
	void* pFileMapView = nullptr;
	hFile = CreateFileU(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;
	defer(CloseHandle(hFile));
	fileSize = GetFileSize(hFile, NULL);
	if (fileSize > (1 << 23))
		return false;
	hFileMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, fileSize, NULL);
	if (!hFileMap)
		return false;
	defer(CloseHandle(hFileMap));
	pFileMapView = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, fileSize);
	if (!pFileMapView)
		return false;
	defer(UnmapViewOfFile(pFileMapView));

	auto exeSize = fileSize;
	auto exeBuffer = pFileMapView;
	if (exeSize < 128)
		return false;
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)exeBuffer;
	if (!pDosHeader || pDosHeader->e_magic != 0x5a4d || (size_t)pDosHeader->e_lfanew + 512 >= exeSize)
		return false;
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((uintptr_t)exeBuffer + pDosHeader->e_lfanew);
	if (!pNtHeader || pNtHeader->Signature != 0x00004550)
		return false;
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNtHeader);
	if (!pSection)
		return false;

	if (pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress != 0 && pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size != 0) {
		auto pExportSectionVA = pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
		for (DWORD i = 0; i < pNtHeader->FileHeader.NumberOfSections; i++, pSection++) {
			if (pSection->VirtualAddress <= pExportSectionVA && pSection->VirtualAddress + pSection->SizeOfRawData > pExportSectionVA) {
				auto pSectionBase = (uintptr_t)exeBuffer - pSection->VirtualAddress + pSection->PointerToRawData;
				PIMAGE_EXPORT_DIRECTORY pExportDirectory = (PIMAGE_EXPORT_DIRECTORY)(pSectionBase + pExportSectionVA);
				char** pExportNames = (char**)(pSectionBase + pExportDirectory->AddressOfNames);
				for (DWORD i = 0; i < pExportDirectory->NumberOfNames; ++i) {
					auto pFunctionName = (char*)(pSectionBase + pExportNames[i]);
					if (!strcmp(pFunctionName, func_name)) {
						return true;
					}
				}
			}
		}
	}

	return false;

#undef MakePointer
}

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
	// Find the NT Header by using the offset of e_lfanew from hMod
	pNTH = (PIMAGE_NT_HEADERS)((UINT_PTR)pDosH + (UINT_PTR)pDosH->e_lfanew);

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
	PIMAGE_NT_HEADERS pNTH;

	assert(directory <= IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR);

	pNTH = GetNtHeader(hMod);
	if(pNTH) {
		UINT_PTR DirVA = pNTH->OptionalHeader.DataDirectory[directory].VirtualAddress;
		if(DirVA) {
			return (BYTE*)hMod + DirVA;
		}
	}
	return NULL;
}

PIMAGE_IMPORT_DESCRIPTOR GetDllImportDesc(HMODULE hMod, const char *dll_name)
{
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc;

	pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)GetNtDataDirectory(hMod, IMAGE_DIRECTORY_ENTRY_IMPORT);
	if(!pImportDesc) {
		return NULL;
	}
	while(pImportDesc->Name) {
		char *name = (char*)((UINT_PTR)hMod + (UINT_PTR)pImportDesc->Name);
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
	pSH = (PIMAGE_SECTION_HEADER)((UINT_PTR)(&pNTH->OptionalHeader) + (UINT_PTR)pNTH->FileHeader.SizeOfOptionalHeader);

	if(!VirtualCheckRegion(pSH, sizeof(IMAGE_SECTION_HEADER) * pNTH->FileHeader.NumberOfSections)) {
		return 0;
	}
	// Search
	for(c = 0; c < pNTH->FileHeader.NumberOfSections; c++) {
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
	DWORD *func_ptrs = (DWORD*)(ExportDesc->AddressOfFunctions + dll_base);
	DWORD *name_ptrs = (DWORD*)(ExportDesc->AddressOfNames + dll_base);
	WORD *name_indices = (WORD*)(ExportDesc->AddressOfNameOrdinals + dll_base);

	exported_func_t* funcs = (exported_func_t*)malloc((ExportDesc->NumberOfFunctions + 1) * sizeof(exported_func_t));
	funcs[ExportDesc->NumberOfFunctions] = {0, 0};

	// i and j can only ever be 16-bit values
	char auto_name[_MAX_ITOSTR_BASE10_COUNT];
	for(WORD i = 0; i < ExportDesc->NumberOfFunctions; i++) {
		const char *name = NULL;

		// Look up name
		for(WORD j = 0; j < ExportDesc->NumberOfNames; j++) {
			if(name_indices[j] == i) {
				name = (const char*)(dll_base + name_ptrs[j]);
				break;
			}
		}
		if(!name) {
			itoa(i + ExportDesc->Base, auto_name, 10);
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

		region_len = (size_t)mbi.BaseAddress + mbi.RegionSize - (size_t)addr;
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
	UINT_PTR *func_ptrs = NULL;
	UINT_PTR *name_ptrs = NULL;
	WORD *name_indices = NULL;
	UINT_PTR i;
	UINT_PTR ordinal = (UINT_PTR)lpProcName;

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
		size_t func_ptrs_size = ExportDesc.NumberOfFunctions * sizeof(UINT_PTR);
		size_t name_ptrs_size = ExportDesc.NumberOfNames * sizeof(UINT_PTR);
		size_t name_indices_size = ExportDesc.NumberOfNames * sizeof(WORD);

		func_ptrs = (UINT_PTR *)malloc(func_ptrs_size);
		name_ptrs = (UINT_PTR *)malloc(name_ptrs_size);
		name_indices = (WORD *)malloc(name_indices_size);
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
		UINT_PTR name_ptr = 0;
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
