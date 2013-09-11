/**
  * Touhou Community Reliant Automatic Patcher
  * Loader
  *
  * ----
  *
  * Parse patch setup and launch game
  */

#include <thcrap.h>
#include <inject.h>

/// Entry point determination
/// -------------------------
// After creating a process in suspended state, EAX is guaranteed to contain
// the correct address of the entry point, even when the executable has the
// DYNAMICBASE flag activated in its header.
//
// (Works on Windows, but not on Wine)
void* entry_from_context(HANDLE hThread)
{
	CONTEXT context;

	ZeroMemory(&context, sizeof(CONTEXT));
	context.ContextFlags = CONTEXT_FULL;
	if(GetThreadContext(hThread, &context)) {
		return (void*)context.Eax;
	}
	return NULL;
}

// Reads the entry point from the PE header.
//
// (Works always, provided that [base_addr] is correct)
void* entry_from_header(HANDLE hProcess, void *base_addr)
{
	void *pe_header = NULL;
	void *ret = NULL;
	MEMORY_BASIC_INFORMATION mbi;
	PIMAGE_DOS_HEADER pDosH;
	PIMAGE_NT_HEADERS pNTH;
	DWORD byte_ret;

	// Read the entire PE header
	if(!VirtualQueryEx(hProcess, base_addr, &mbi, sizeof(mbi))) {
		goto end;
	}
	pe_header = malloc(mbi.RegionSize);
	ReadProcessMemory(hProcess, base_addr, pe_header, mbi.RegionSize, &byte_ret);
	pDosH = (PIMAGE_DOS_HEADER)pe_header;

	// Verify that the PE is valid by checking e_magic's value
	if(pDosH->e_magic != IMAGE_DOS_SIGNATURE) {
		goto end;
	}

	// Find the NT Header by using the offset of e_lfanew value from hMod
	pNTH = (PIMAGE_NT_HEADERS) ((DWORD) pDosH + (DWORD) pDosH->e_lfanew);

	// Verify that the NT Header is correct
	if(pNTH->Signature != IMAGE_NT_SIGNATURE) {
		goto end;
	}

	// Alright, we have the entry point now
	ret = (void*)(pNTH->OptionalHeader.AddressOfEntryPoint + (DWORD)base_addr);
end:
	SAFE_FREE(pe_header);
	return ret;
}

// Returns the base address of the module with the given title in [hProcess].
//
// (Works on Wine, but not on Windows immediately after the target process was
//  created in suspended state.)
void* module_base_get(HANDLE hProcess, const char *module) 
{ 
	HMODULE *hModules = NULL;
	DWORD cModules; 
	void *ret = NULL;
	//------ 

	EnumProcessModules(hProcess, hModules, 0, &cModules); 
	hModules = (HMODULE*)malloc(cModules); 

	if(EnumProcessModules(hProcess, hModules, cModules / sizeof(HMODULE), &cModules)) {
		size_t i;
		for(i = 0; i < cModules / sizeof(HMODULE); i++) { 
			char cur_module[MAX_PATH];
			if(GetModuleFileNameEx(hProcess, hModules[i], cur_module, sizeof(cur_module))) {
				if(!strcmp(module, cur_module)) { 
					ret = hModules[i]; 
					break; 
				}
			} 
		} 
	}
	SAFE_FREE(hModules);
	return ret;
}
/// -------------------------

int ThreadWaitUntil(HANDLE hProcess, HANDLE hThread, void *addr)
{
	CONTEXT context;
	BYTE entry_asm_orig[2];
	const BYTE entry_asm_delay[2] = {0xEB, 0xFE}; // JMP SHORT YADA YADA
	MEMORY_BASIC_INFORMATION mbi;
	DWORD byte_ret;
	DWORD old_prot;

	if(!VirtualQueryEx(hProcess, addr, &mbi, sizeof(mbi))) {
		return 1;
	}
	VirtualProtectEx(hProcess, mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &old_prot);
	ReadProcessMemory(hProcess, addr, entry_asm_orig, sizeof(entry_asm_orig), &byte_ret);
	WriteProcessMemory(hProcess, addr, entry_asm_delay, sizeof(entry_asm_delay), &byte_ret);
	FlushInstructionCache(hProcess, addr, sizeof(entry_asm_delay));
	VirtualProtectEx(hProcess, mbi.BaseAddress, mbi.RegionSize, old_prot, &old_prot);

	ZeroMemory(&context, sizeof(CONTEXT));
	context.ContextFlags = CONTEXT_CONTROL;
	while(context.Eip != (DWORD)addr) {
		ResumeThread(hThread);
		Sleep(10);
		SuspendThread(hThread);
		GetThreadContext(hThread, &context);
	}

	// Write back the original code
	WriteProcessMemory(hProcess, addr, entry_asm_orig, sizeof(entry_asm_orig), &byte_ret);
	FlushInstructionCache(hProcess, addr, sizeof(entry_asm_orig));
	return 0;
}

int WaitUntilEntryPoint(HANDLE hProcess, HANDLE hThread, const char *module)
{
	// Try to get the entry point by various means, sorted by both efficiency
	// and probability of them working.
	void *entry_addr = NULL;

	if(!(entry_addr = entry_from_context(hThread))) {
		void *module_base;

		if(!(module_base = module_base_get(hProcess, module))) {
			// shit, gotta take a guess and hope it works
			module_base = (void*)0x400000;
		}
		entry_addr = entry_from_header(hProcess, module_base);
	}

	if(entry_addr) {
		return ThreadWaitUntil(hProcess, hThread, entry_addr);
	} else {
		log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
			"Couldn't determine the entry point of %s!\n"
			"\n"
			"Seems as if %s won't work with this game on your system.\n",
			PathFindFileNameA(module), PROJECT_NAME_SHORT()
		);
		return 1;
	}
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	int ret;

	json_t *games_js = NULL;

	const char *run_cfg_fn = NULL;
	json_t *run_cfg = NULL;

	const char *cmd_exe_fn = NULL;
	const char *cfg_exe_fn = NULL;
	const char *final_exe_fn = NULL;

	json_t *args = json_array();

	int i;

	if(__argc < 2) {
		log_mboxf(NULL, MB_OK | MB_ICONINFORMATION,
			"This is the command-line loader component of the %s.\n"
			"\n"
			"You are probably looking for the configuration tool to set up shortcuts for the simple usage of the patcher - that would be thcrap_configure.\n"
			"\n"
			"If not, this is how to use the loader directly:\n"
			"\n"
			"\tthcrap_loader runconfig.js [game_id] [game.exe]\n"
			"\n"
			"- The run configuration file must end in .js\n"
			"- If [game_id] is given, the location of the game executable is read from games.js\n"
			"- Later command-line parameters take priority over earlier ones\n",
			PROJECT_NAME()
		);
		ret = -1;
		goto end;
	}

	// Convert command-line arguments
	for(i = 0; i < __argc; i++) {
		size_t arg_len = (wcslen(__wargv[i]) * UTF8_MUL) + 1;
		VLA(char, arg, arg_len);
		StringToUTF8(arg, __wargv[i], arg_len);
		json_array_append_new(args, json_string(arg));
		VLA_FREE(arg);
	}

	/** 
	  * ---
	  * "Activate AppLocale layer in case it's installed."
	  *
	  * Seemed like a good idea.
	  * On Vista and above however, this loads apphelp.dll into our process.
	  * This DLL sandboxes our application by hooking GetProcAddress and a whole
	  * bunch of other functions.
	  * In turn, thcrap_inject gets wrong pointers to the system functions used in
	  * the injection process, crashing the game as soon as it calls one of those.
	  * ----
	  */
	// SetEnvironmentVariable(L"__COMPAT_LAYER", L"ApplicationLocale");
	// SetEnvironmentVariable(L"AppLocaleID", L"0411");

	// Load games.js
	{
		size_t games_js_fn_len = GetModuleFileNameU(NULL, NULL, 0) + 1 + strlen("games.js") + 1;
		VLA(char, games_js_fn, games_js_fn_len);

		GetModuleFileNameU(NULL, games_js_fn, games_js_fn_len);
		PathRemoveFileSpec(games_js_fn);
		PathAddBackslashA(games_js_fn);
		strcat(games_js_fn, "games.js");
		games_js = json_load_file_report(games_js_fn);
		VLA_FREE(games_js_fn);
	}

	// Parse command line
	for(i = 1; i < __argc; i++) {
		const char *arg = json_array_get_string(args, i);
		const char *param_ext = PathFindExtensionA(arg);
		const char *new_exe_fn = NULL;

		if(!stricmp(param_ext, ".js")) {
			const char *game = NULL;

			// Sorry guys, no run configuration stacking yet
			if(json_is_object(run_cfg)) {
				json_decref(run_cfg);
			}
			run_cfg_fn = arg;
			run_cfg = json_load_file_report(run_cfg_fn);

			// First, evaluate runconfig values, then command line overrides
			game = json_object_get_string(run_cfg, "game");
			new_exe_fn = json_object_get_string(games_js, game);

			if(!new_exe_fn) {
				new_exe_fn = json_object_get_string(run_cfg, "exe");
			}
			if(new_exe_fn) {
				cfg_exe_fn = new_exe_fn;
			}
		} else if(!stricmp(param_ext, ".exe")) {
			cmd_exe_fn = arg;
		} else if(games_js) {
			cmd_exe_fn = json_object_get_string(games_js, arg);
		}
	}

	if(!run_cfg) {
		log_mbox(NULL, MB_OK | MB_ICONEXCLAMATION,
			"No valid run configuration file given!\n"
			"\n"
			"If you do not have one yet, use the thcrap_configure tool to create one.\n"
		);
		ret = -2;
		goto end;
	}

	final_exe_fn = cmd_exe_fn ? cmd_exe_fn : cfg_exe_fn;

	/*
	// Recursively apply the passed runconfigs
	for(i = 1; i < __argc; i++)
	{
		json_t *cur_cfg = json_load_file_report(args[i]);
		if(!cur_cfg) {
			continue;
		}
		if(!run_cfg) {
			run_cfg = cur_cfg;
		} else {
			json_object_merge(run_cfg, cur_cfg);

		}
	}
	*/
	// Still none?
	if(!final_exe_fn) {
		log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
			"No game executable file given!\n"
			"\n"
			"This could either be given as a command line argument "
			"(just add something ending in .exe), "
			"or as an \"exe\" value in your run configuration (%s).\n",
			run_cfg_fn
		);
		ret = -3;
		goto end;
	}
	{
		STRLEN_DEC(final_exe_fn);
		VLA(char, game_dir, final_exe_fn_len);
		VLA(char, final_exe_fn_local, final_exe_fn_len);

		STARTUPINFOA si;
		PROCESS_INFORMATION pi;

		ZeroMemory(&si, sizeof(STARTUPINFOA));
		ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

		strcpy(final_exe_fn_local, final_exe_fn);
		str_slash_normalize_win(final_exe_fn_local);

		strcpy(game_dir, final_exe_fn);
		PathRemoveFileSpec(game_dir);

		ret = CreateProcess(
			final_exe_fn_local, game_dir, NULL, NULL, TRUE,
			CREATE_SUSPENDED, NULL, game_dir, &si, &pi
		);
		if(ret) {
			if(!WaitUntilEntryPoint(pi.hProcess, pi.hThread, final_exe_fn_local)) {
				thcrap_inject(pi.hProcess, run_cfg_fn);
			}
			ResumeThread(pi.hThread);
		} else {
			char *msg_str;

			ret = GetLastError();
		
			FormatMessage(
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, ret, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPSTR)&msg_str, 0, NULL
			);

			log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
				"Failed to start %s: %s",
				final_exe_fn, msg_str
			);
			LocalFree(msg_str);
			ret = -4;
			goto end;
		}
		VLA_FREE(game_dir);
		VLA_FREE(final_exe_fn_local);
	}
	ret = 0;
end:
	json_decref(games_js);
	json_decref(run_cfg);
	json_decref(args);
	return ret;
}
