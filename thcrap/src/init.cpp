/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * DLL and engine initialization.
  */

#include "thcrap.h"
#include "sha256.h"
#include "win32_detour.h"

// Both thcrap_ExitProcess and DllMain will call ExitDll,
// which will cause a crash without this workaround
static bool dll_exited = false;

/// Static global variables
/// -----------------------
// Required to get the exported functions of thcrap.dll.
static HMODULE hThcrap = NULL;
static char *dll_dir = NULL;
static size_t stage_cur = 0;
static std::vector<bool> bp_set; // One per stage
/// -----------------------

/// Old game build message
/// ----------------------
static const char oldbuild_title[] = "Old version detected";

static const char oldbuild_header[] =
	"You are running an old version of ${game_title} (${build_running}).\n"
	"\n";

static const char oldbuild_maybe_supported[] =
	"${project_short} may or may not work with this version, so we recommend updating to the latest official version (${build_latest}).";

static const char oldbuild_not_supported[] =
	"${project_short} will *not* work with this version. Please update to the latest official version, ${build_latest}.";

static const char oldbuild_url[] =
	"\n"
	"\n"
	"You can download the update at\n"
	"\n"
	"\t${url_update}\n"
	"\n"
	"(Press Ctrl+C to copy the text of this message box and the URL)";

void oldbuild_show()
{
	const char *title = runconfig_title_get();

	if(!title) {
		return;
	}
	if(runconfig_latest_check() == false) {
		const size_t MSG_SLOT = (size_t)oldbuild_header;
		unsigned int msg_type = MB_ICONINFORMATION;
		const char *url_update = runconfig_update_url_get();

		// Try to find out whether this version actually is supported or
		// not, by looking for a <game>.<version>.js file in the stack.
		// There are a number of drawbacks to this approach, though:
		// • It can't tell the *amount* of work that actually was done
		//   for this specific version.
		// • What if one patch in the stack *does* provide support for
		//   this older version, but others don't?
		// • Stringlocs are also part of support. -.-
		const char BUILD_JS_FORMAT[] = "%s.%s.js";
		const char *game_str = runconfig_game_get();
		const char *build_str = runconfig_build_get();
		const int build_js_fn_len = snprintf(NULL, 0, BUILD_JS_FORMAT, game_str, build_str) + 1;
		VLA(char, build_js_fn, build_js_fn_len);
		sprintf(build_js_fn, BUILD_JS_FORMAT, game_str, build_str);
		char *build_js_chain[] = {
			build_js_fn,
			nullptr
		};
		json_t* build_js = stack_json_resolve_chain(build_js_chain, nullptr);
		VLA_FREE(build_js_fn);
		const bool supported = build_js != nullptr;
		json_decref(build_js);

		strings_strclr(MSG_SLOT);
		strings_strcat(MSG_SLOT, oldbuild_header);
		if(supported) {
			strings_strcat(MSG_SLOT, oldbuild_maybe_supported);
		}
		else {
			msg_type = MB_ICONEXCLAMATION;
			strings_strcat(MSG_SLOT, oldbuild_not_supported);
		}
		if(url_update) {
			strings_strcat(MSG_SLOT, oldbuild_url);
		}

		strings_replace(MSG_SLOT, "${game_title}", title);
		strings_replace(MSG_SLOT, "${build_running}", build_str);
		strings_replace(MSG_SLOT, "${build_latest}", runconfig_latest_get());
		strings_replace(MSG_SLOT, "${project_short}", PROJECT_NAME_SHORT);
		const char *msg = strings_replace(MSG_SLOT, "${url_update}", url_update);

		log_mbox(oldbuild_title, MB_OK | msg_type, msg);
	}
}
/// -------------------

game_version* identify_by_hash(const char *fn, size_t *file_size, json_t *versions)
{
	if (json_t* json_hashes = json_object_get(versions, "hashes")) {
		if (unsigned char* file_buffer = (unsigned char*)file_read(fn, file_size)) {

			SHA256_HASH hash = sha256_calc(file_buffer, *file_size);
			free(file_buffer);

			char hash_str[65];
			for (size_t i = 0; i < 8; i++) {
				sprintf(hash_str + (i * 8), "%08x", _byteswap_ulong(hash.dwords[i]));
			}
			json_t *id = json_object_get(json_hashes, hash_str);
			return id ? new game_version(id) : nullptr;
		}
	}
	return nullptr;
}

game_version* identify_by_size(size_t file_size, json_t *versions)
{
	json_t *id = json_object_numkey_get(json_object_get(versions, "sizes"), file_size);
	return id ? new game_version(id) : nullptr;
}

void identify_free(game_version *ver)
{
	if (!ver)
		return;

	free(ver->id);
	free(ver->build);
	free(ver->variety);
	delete ver;
}

json_t* stack_cfg_resolve(const char *fn, size_t *file_size)
{
	json_t *ret = json_object();
	char **chain = resolve_chain(fn);
	if(chain && chain[0]) {
		std::vector<const char*> new_chain;
		new_chain.push_back("global.js");
		for (size_t i = 0; chain[i]; i++) {
			new_chain.push_back(chain[i]);
		}

		log_printf("(JSON) Resolving configuration for %s... ", fn);
		size_t tmp_file_size = 0;
		stack_foreach_cpp([&new_chain, &ret, &tmp_file_size](const patch_t *patch) {
			json_t *json_new = json_object();
			for (const char *&file : new_chain) {
				tmp_file_size += patch_json_merge(&json_new, patch, file);
			}
			json_object_update_recursive(json_new, patch->config);
			json_object_update_recursive(ret, json_new);
			json_decref(json_new);
		});
		log_print(tmp_file_size ? "\n" : "not found\n");
		if (file_size) *file_size = tmp_file_size;
	}
	chain_free(chain);
	return ret;
}

json_t* identify(const char *exe_fn)
{
	size_t exe_size;
	json_t *run_ver = NULL;
	json_t *versions_js = stack_json_resolve("versions.js", NULL);

	// Result of the EXE identification
	game_version *id = NULL;
	bool size_cmp = false;

	if(!versions_js) {
		goto end;
	}
	log_print("Hashing executable... ");

	id = identify_by_hash(exe_fn, &exe_size, versions_js);
	if(!id) {
		size_cmp = true;
		log_print(
			"failed!\n"
			"File size lookup... "
		);
		id = identify_by_size(exe_size, versions_js);

		if(!id) {
			log_print("failed!\n");
			goto end;
		}
	}

	if(!id->id || !id->build) {
		log_print("Invalid version format!");
		goto end;
	}

	if(id->codepage) {
		w32u8_set_fallback_codepage(id->codepage);
	}

	// Store build in the runconfig to be recalled later for version-
	// dependent patch file resolving. Needs to be directly written to
	// run_cfg because we already require it down below to resolve ver_fn.
	runconfig_build_set(id->build);

	log_printf("→ %s %s %s (codepage %d)\n", id->id, id->build, id->variety, id->codepage);

	if(stricmp(PathFindExtensionA(id->id), ".js") != 0) {
		std::string ver_fn = std::string(id->id) + ".js";
		run_ver = stack_cfg_resolve(ver_fn.c_str(), NULL);
	} else {
		run_ver = stack_cfg_resolve(id->id, NULL);
	}

	// Ensure that we have a configuration with a "game" key
	if(!run_ver) {
		run_ver = json_object();
	}
	if(!json_object_get_string(run_ver, "game")) {
		json_object_set_new(run_ver, "game", json_string(id->id));
	}

	if(size_cmp) {
		const char *game_title = json_object_get_string(run_ver, "title");
		int ret;
		if(!game_title) {
			game_title = id->id;
		}
		ret = log_mboxf("Unrecognized version detected", MB_YESNO | MB_ICONQUESTION,
			"You have attached %s to an unrecognized game version.\n"
			"According to the file size, this is most likely\n"
			"\n"
			"%s %s %s\n"
			"\n"
			"but we haven't tested this exact variety yet and thus can't confirm that the patches will work.\n"
			"They might crash the game, damage your save files or cause even worse problems.\n"
			"\n"
			"Please post <%s> in one of the following places:\n"
			"\n"
			"• Discord: http://discord.thpatch.net\n"
			"• IRC: #thcrap on irc.libera.chat:6697. Webchat at https://web.libera.chat/?channels=#thcrap\n"
			"\n"
			"We will take a look at it, and add support if possible.\n"
			"\n"
			"Apply patches for the identified game version regardless (on your own risk)?",
			PROJECT_NAME_SHORT, id->id, id->build, id->variety, exe_fn
		);
		if(ret == IDNO) {
			run_ver = json_decref_safe(run_ver);
		}
	}
end:
	json_decref(versions_js);
	identify_free(id);
	return run_ver;
}

void thcrap_detour(HMODULE hProc)
{
	size_t mod_name_len = GetModuleFileNameU(hProc, NULL, 0) + 1;
	VLA(char, mod_name, mod_name_len);
	GetModuleFileNameU(hProc, mod_name, mod_name_len);
	log_printf("Applying %s detours to %s...\n", PROJECT_NAME_SHORT, mod_name);

	iat_detour_apply(hProc);
	VLA_FREE(mod_name);
}

int thcrap_init(const char *run_cfg)
{
	bool perf_tested = true;
	LARGE_INTEGER begin_time;
	if (!QueryPerformanceCounter(&begin_time)) {
		perf_tested = false;
	}

	size_t exe_fn_len = GetModuleFileNameU(NULL, NULL, 0) + 1;
	size_t game_dir_len = GetCurrentDirectory(0, NULL) + 1;
	VLA(char, exe_fn, exe_fn_len);
	VLA(char, game_dir, game_dir_len);

	GetModuleFileNameU(NULL, exe_fn, exe_fn_len);
	GetCurrentDirectory(game_dir_len, game_dir);
	SetCurrentDirectory(dll_dir);

	globalconfig_init();
	exception_load_config();
	log_init(globalconfig_get_boolean("console", false));

	if (*run_cfg != '{') {
		runconfig_load_from_file(run_cfg);
	} else {
		runconfig_load(json_loads(run_cfg, 0, nullptr), RUNCFG_STAGE_DEFAULT);
	}

	runconfig_thcrap_dir_set(dll_dir);

	stack_show_missing();

	log_printf("EXE file name: %s\n", exe_fn);
	if (json_t *full_cfg = identify(exe_fn)) {
		runconfig_load(full_cfg, RUNCONFIG_NO_OVERWRITE);
		json_decref(full_cfg);

		oldbuild_show();
	}

	log_printf("Game directory: %s\\\n", game_dir);

	PathAppendU(dll_dir, "bin");
	log_printf(
		"Plug-in directory: %s\\\n"
		"\nInitializing plug-ins...\n"
		, dll_dir
	);
	plugin_init(hThcrap);
	
	plugins_load(dll_dir);
	PathAppendU(dll_dir, "..");

	/**
	  * Potentially dangerous stuff. Do not want!
	  */
	/*
	{
		json_t *patches = json_object_get(run_cfg, "patches");
		json_t *patch_info;

		json_array_foreach_scoped(size_t, i, patches, patch_info) {
			if(const char* archive = json_object_get_string(patch_info, "archive")) {
				plugins_load(archive);
			}
		}
	}
	*/

	// We might want to move this to thcrap_init_binary() too to accommodate
	// DRM that scrambles the original import table, but since we're not
	// having any test cases right now...
	thcrap_detour(CurrentModuleHandle);

	SetCurrentDirectory(game_dir);
	VLA_FREE(game_dir);
	VLA_FREE(exe_fn);
	bp_set.resize(runconfig_stage_count(), false);

	int ret = thcrap_init_binary(0, nullptr);

	if (perf_tested) {
		LARGE_INTEGER end_time;
		LARGE_INTEGER perf_freq;
		QueryPerformanceFrequency(&perf_freq);
		QueryPerformanceCounter(&end_time);
		double time = (double)(end_time.QuadPart - begin_time.QuadPart) / perf_freq.QuadPart;
		log_printf(
			"---------------------------\n"
			"Initialization completed in %f seconds\n"
			"---------------------------\n"
			, time
		);
	} else {
		log_print(
			"---------------------------\n"
			"Initialization completed, but measuring performance failed\n"
			"---------------------------\n"
		);
	}

	return ret;
}

int BP_init_next_stage(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	auto module = (HMODULE)json_object_get_immediate(bp_info, regs, "module");
	// ----------
	thcrap_init_binary(++stage_cur, module);
	return 1;
}

int thcrap_init_binary(size_t stage_num, HMODULE module)
{
	size_t stages_total = runconfig_stage_count();

	assert(bp_set.size() == stages_total);

	if (stage_num < stages_total) {
		--stages_total; // Offset by 1 to get last stage index
		size_t stages_remaining = stages_total - stage_num;

		if (stages_remaining != 0) {
			log_printf(
				"Initialization stage %d...\n"
				"-------------------------\n",
				stage_num
			);
		}

		bool stage_success = runconfig_stage_apply(stage_num,
			RUNCFG_STAGE_USE_MODULE | (bp_set[stage_num] ? RUNCFG_STAGE_SKIP_BREAKPOINTS : 0),
			module);

		if (stage_success == false && stages_remaining != 0) {
			log_printf(
				"Failed. Jumping to last stage...\n"
				"-------------------------\n"
			);
			return thcrap_init_binary(stages_total, nullptr);
		}
	}
	runconfig_print();
	mod_func_run_all("post_init", NULL);
	return 0;
}

int InitDll(HMODULE hDll)
{
	HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
	json_set_alloc_funcs(malloc, free);

	// Store the thcrap directory to load plug-ins later
	// And also load config.js to set fallback codepage
	dll_dir = (char*)malloc(MAX_PATH);
	GetModuleFileNameU(hDll, dll_dir, MAX_PATH);
	PathAppendU(dll_dir, "..\\..");
	PathAddBackslashU(dll_dir);
	wchar_t prev_dir[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, prev_dir);
	SetCurrentDirectoryU(dll_dir);
	w32u8_set_fallback_codepage(globalconfig_get_integer("codepage", 932));
	SetCurrentDirectoryW(prev_dir);
	
	exception_init();
	// Needs to be at the lowest level
	win32_detour();
	detour_chain("kernel32.dll", 0,
		"ExitProcess", thcrap_ExitProcess,
		NULL
	);

	hThcrap = hDll;

	return 0;
}

void ExitDll()
{
	dll_exited = true;
	// Yes, the main thread does not receive a DLL_THREAD_DETACH message
	mod_func_run_all("thread_exit", NULL);
	mod_func_run_all("exit", NULL);
	plugins_close();
	runconfig_free();

	SAFE_FREE(dll_dir);
#if defined(_MSC_VER) && defined(_DEBUG)
	// Called in RawDllMain() in this configuration
#else
	log_exit();
#endif
}

VOID WINAPI thcrap_ExitProcess(UINT uExitCode)
{
	ExitDll();
	// The detour cache is already freed at this point, and this will
	// always be the final detour in the chain, so detour_next() doesn't
	// make any sense here (and would leak memory as well).
	ExitProcess(uExitCode);
}

BOOL APIENTRY DllMain(HMODULE hDll, DWORD ulReasonForCall, LPVOID)
{
	switch(ulReasonForCall) {
		case DLL_PROCESS_ATTACH:
			InitDll(hDll);
			break;
		case DLL_PROCESS_DETACH:
			if (!dll_exited) {
				ExitDll();
			}
			break;
		case DLL_THREAD_DETACH:
			mod_func_run_all("thread_exit", NULL);
			break;
	}
	return TRUE;
}

// Visual Studio's CRT registers destructors for STL containers with global
// lifetime using atexit(), which is run *after* DllMain(DLL_PROCESS_DETACH),
// so we shouldn't report memory leaks there. Registering our own atexit()
// handler in DllMain(DLL_PROCESS_ATTACH) unfortunately pushes it onto the top
// of the atexit() stack, which means it gets run *first* before all of the
// destructors, so that's not an option either.
// Thankfully, the CRT provides a separate, undocumented (?) hook in
// _pRawDllMain, which does indeed run after all of those.
#if defined(_MSC_VER) && defined(_DEBUG)
extern "C" {

	BOOL APIENTRY RawDllMain(HMODULE hDll, DWORD ulReasonForCall, LPVOID)
	{
		switch(ulReasonForCall) {
		case DLL_PROCESS_DETACH:
			_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
			_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
			_CrtDumpMemoryLeaks();
			log_exit();
			break;
		}
		return TRUE;
	}

	auto *_pRawDllMain = RawDllMain;
}
#endif
