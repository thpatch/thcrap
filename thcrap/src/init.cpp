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

/// Static global variables
/// -----------------------
// Required to get the exported functions of thcrap.dll.
static HMODULE hThcrap = NULL;
static char *dll_dir = NULL;
breakpoint_set_t *bp_set = nullptr; // One per stage
size_t stage_cur = 0;
size_t stages_total = 0; // Including the run configuration, therefore >= 1
/// -----------------------

/// Old game build message
/// ----------------------
const char *oldbuild_title = "Old version detected";

const char *oldbuild_header =
	"You are running an old version of ${game_title} (${build_running}).\n"
	"\n";

const char *oldbuild_maybe_supported =
	"${project_short} may or may not work with this version, so we recommend updating to the latest official version (${build_latest}).";

const char *oldbuild_not_supported =
	"${project_short} will *not* work with this version. Please update to the latest official version, ${build_latest}.";

const char *oldbuild_url =
	"\n"
	"\n"
	"You can download the update at\n"
	"\n"
	"\t${url_update}\n"
	"\n"
	"(Press Ctrl+C to copy the text of this message box and the URL)";

int IsLatestBuild(json_t *build, json_t **latest)
{
	json_t *json_latest = json_object_get(runconfig_get(), "latest");
	size_t i;
	if(!json_is_string(build) || !latest || !json_latest) {
		return -1;
	}
	json_flex_array_foreach(json_latest, i, *latest) {
		if(json_equal(build, *latest)) {
			return 1;
		}
	}
	return 0;
}

int oldbuild_show(json_t *run_cfg)
{
	const json_t *title = runconfig_title_get();
	json_t *build = json_object_get(run_cfg, "build");
	json_t *latest = NULL;
	int ret;

	if(!json_is_string(build) || !json_is_string(title)) {
		return -1;
	}
	ret = IsLatestBuild(build, &latest) == 0 && json_is_string(latest);
	if(ret) {
		const size_t MSG_SLOT = (size_t)oldbuild_header;
		unsigned int msg_type = MB_ICONINFORMATION;
		auto *url_update = json_object_get_string(run_cfg, "url_update");

		// Try to find out whether this version actually is supported or
		// not, by looking for a <game>.<version>.js file in the stack.
		// There are a number of drawbacks to this approach, though:
		// • It can't tell the *amount* of work that actually was done
		//   for this specific version.
		// • What if one patch in the stack *does* provide support for
		//   this older version, but others don't?
		// • Stringlocs are also part of support. -.-
		const auto *BUILD_JS_FORMAT = "%s.%s.js";
		auto *game_str = json_object_get_string(run_cfg, "game");
		auto *build_str = json_string_value(build);
		auto build_js_fn_len = _scprintf(BUILD_JS_FORMAT, game_str, build_str) + 1;
		VLA(char, build_js_fn, build_js_fn_len);
		sprintf(build_js_fn, BUILD_JS_FORMAT, game_str, build_str);
		auto *build_js_chain = json_pack("[s]", build_js_fn);
		auto *build_js = stack_json_resolve_chain(build_js_chain, nullptr, nullptr);
		auto supported = build_js != nullptr;
		json_decref_safe(build_js);
		json_decref_safe(build_js_chain);
		VLA_FREE(build_js_fn);

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

		strings_replace(MSG_SLOT, "${game_title}", json_string_value(title));
		strings_replace(MSG_SLOT, "${build_running}", json_string_value(build));
		strings_replace(MSG_SLOT, "${build_latest}", json_string_value(latest));
		strings_replace(MSG_SLOT, "${project_short}", PROJECT_NAME_SHORT());
		auto *msg = strings_replace(MSG_SLOT, "${url_update}", url_update);

		log_mbox(oldbuild_title, MB_OK | msg_type, msg);
	}
	return ret;
}
/// -------------------

json_t* identify_by_hash(const char *fn, size_t *file_size, json_t *versions)
{
	unsigned char *file_buffer = (unsigned char*)file_read(fn, file_size);
	SHA256_CTX sha256_ctx;
	BYTE hash[32];
	char hash_str[65];
	int i;

	if(!file_buffer) {
		return NULL;
	}
	sha256_init(&sha256_ctx);
	sha256_update(&sha256_ctx, file_buffer, *file_size);
	sha256_final(&sha256_ctx, hash);
	SAFE_FREE(file_buffer);

	for(i = 0; i < 32; i++) {
		sprintf(hash_str + (i * 2), "%02x", hash[i]);
	}
	return json_object_get(json_object_get(versions, "hashes"), hash_str);
}

json_t* identify_by_size(size_t file_size, json_t *versions)
{
	return json_object_numkey_get(json_object_get(versions, "sizes"), file_size);
}

json_t* stack_cfg_resolve(const char *fn, size_t *file_size)
{
	json_t *ret = NULL;
	json_t *chain = resolve_chain(fn);
	if(json_array_size(chain)) {
		json_array_insert_new(chain, 0, json_string("global.js"));
		log_printf("(JSON) Resolving configuration for %s... ", fn);
		ret = stack_json_resolve_chain(chain, file_size, nullptr);
	}
	json_decref(chain);
	return ret;
}

json_t* identify(const char *exe_fn)
{
	size_t exe_size;
	json_t *run_ver = NULL;
	json_t *versions_js = stack_json_resolve("versions.js", NULL);
	json_t *game_obj = NULL;
	json_t *build_obj = NULL;
	json_t *variety_obj = NULL;
	json_t *codepage_obj = NULL;
	const char *game = NULL;
	const char *build = NULL;
	const char *variety = NULL;
	UINT codepage;

	// Result of the EXE identification
	json_t *id_array = NULL;
	int size_cmp = 0;

	if(!versions_js) {
		goto end;
	}
	log_printf("Hashing executable... ");

	id_array = identify_by_hash(exe_fn, &exe_size, versions_js);
	if(!id_array) {
		size_cmp = 1;
		log_printf("failed!\n");
		log_printf("File size lookup... ");
		id_array = identify_by_size(exe_size, versions_js);

		if(!id_array) {
			log_printf("failed!\n");
			goto end;
		}
	}

	game_obj = json_array_get(id_array, 0);
	build_obj = json_array_get(id_array, 1);
	variety_obj = json_array_get(id_array, 2);
	codepage_obj = json_array_get(id_array, 3);
	game = json_string_value(game_obj);
	build = json_string_value(build_obj);
	variety = json_string_value(variety_obj);
	codepage = json_hex_value(codepage_obj);

	if(!game || !build) {
		log_printf("Invalid version format!");
		goto end;
	}

	if(codepage) {
		w32u8_set_fallback_codepage(codepage);
	}

	// Store build in the runconfig to be recalled later for version-
	// dependent patch file resolving. Needs to be directly written to
	// run_cfg because we already require it down below to resolve ver_fn.
	json_object_set(run_cfg, "build", build_obj);

	// More robust than putting the UTF-8 character here directly,
	// who knows which locale this might be compiled under...
	log_printf("\xE2\x86\x92 %s %s %s (codepage %d)\n", game, build, variety, codepage);

	if(stricmp(PathFindExtensionA(game), ".js")) {
		size_t ver_fn_len = json_string_length(game_obj) + 1 + strlen(".js") + 1;
		VLA(char, ver_fn, ver_fn_len);
		sprintf(ver_fn, "%s.js", game);
		run_ver = stack_cfg_resolve(ver_fn, NULL);
		VLA_FREE(ver_fn);
	} else {
		run_ver = stack_cfg_resolve(game, NULL);
	}

	// Ensure that we have a configuration with a "game" key
	if(!run_ver) {
		run_ver = json_object();
	}
	if(!json_object_get_string(run_ver, "game")) {
		json_object_set(run_ver, "game", game_obj);
	}

	if(size_cmp) {
		const char *game_title = json_object_get_string(run_ver, "title");
		int ret;
		if(game_title) {
			game = game_title;
		}
		ret = log_mboxf("Unknown version detected", MB_YESNO | MB_ICONQUESTION,
			"You have attached %s to an unknown game version.\n"
			"According to the file size, this is most likely\n"
			"\n"
			"\t%s %s %s\n"
			"\n"
			"but we haven't tested this exact variety yet and thus can't confirm that the patches will work.\n"
			"They might crash the game, damage your save files or cause even worse problems.\n"
			"\n"
			"Please post <%s> in one of the following places:\n"
			"\n"
			"• Discord: http://discord.thpatch.net\n"
			"• IRC: #thcrap on irc.freenode.net. Webchat at https://webchat.freenode.net/?channels=#thcrap\n"
			"\n"
			"We will take a look at it, and add support if possible.\n"
			"\n"
			"Apply patches for the identified game version regardless (on your own risk)?",
			PROJECT_NAME_SHORT(), game, build, variety, exe_fn
		);
		if(ret == IDNO) {
			run_ver = json_decref_safe(run_ver);
		}
	}
end:
	json_decref(versions_js);
	return run_ver;
}

void thcrap_detour(HMODULE hProc)
{
	size_t mod_name_len = GetModuleFileNameU(hProc, NULL, 0) + 1;
	VLA(char, mod_name, mod_name_len);
	GetModuleFileNameU(hProc, mod_name, mod_name_len);
	log_printf("Applying %s detours to %s...\n", PROJECT_NAME_SHORT(), mod_name);

	iat_detour_apply(hProc);
	VLA_FREE(mod_name);
}

int thcrap_init(const char *run_cfg_fn)
{
	json_t *user_cfg = NULL;
	HMODULE hProc = GetModuleHandle(NULL);

	size_t exe_fn_len = GetModuleFileNameU(NULL, NULL, 0) + 1;
	size_t game_dir_len = GetCurrentDirectory(0, NULL) + 1;
	VLA(char, exe_fn, exe_fn_len);
	VLA(char, game_dir, game_dir_len);

	GetModuleFileNameU(NULL, exe_fn, exe_fn_len);
	GetCurrentDirectory(game_dir_len, game_dir);
	PathAppendU(dll_dir, "..");
	SetCurrentDirectory(dll_dir);

	user_cfg = json_load_file_report(run_cfg_fn);
	runconfig_set(user_cfg);

	{
		json_t *console_val = json_object_get(run_cfg, "console");
		log_init(json_is_true(console_val));
	}

	json_object_set_new(run_cfg, "thcrap_dir", json_string(dll_dir));
	json_object_set_new(run_cfg, "run_cfg_fn", json_string(run_cfg_fn));
	log_printf("Run configuration file: %s\n\n", run_cfg_fn);

	log_printf("Initializing patches...\n");
	patches_init(run_cfg_fn);
	stack_show_missing();

	log_printf("EXE file name: %s\n", exe_fn);
	{
		json_t *full_cfg = identify(exe_fn);
		if(full_cfg) {
			json_object_merge(full_cfg, user_cfg);
			runconfig_set(full_cfg);
			json_decref(full_cfg);

			oldbuild_show(full_cfg);
		}
	}

	log_printf("Game directory: %s\n", game_dir);
	log_printf("Plug-in directory: %s\n", dll_dir);

	log_printf("\nInitializing plug-ins...\n");
	plugin_init(hThcrap);
	PathAppendU(dll_dir, "bin");
	plugins_load(dll_dir);
	PathAppendU(dll_dir, "..");

	/**
	  * Potentially dangerous stuff. Do not want!
	  */
	/*
	{
		json_t *patches = json_object_get(run_cfg, "patches");
		size_t i = 1;
		json_t *patch_info;

		json_array_foreach(patches, i, patch_info) {
			const char *archive = json_object_get_string(patch_info, "archive");
			if(archive) {
				plugins_load(archive);
			}
		}
	}
	*/

	// We might want to move this to thcrap_init_binary() too to accommodate
	// DRM that scrambles the original import table, but since we're not
	// having any test cases right now...
	thcrap_detour(hProc);

	// Init stages
	// -----------
	auto init_stages = json_object_get(run_cfg, "init_stages");
	stages_total = json_array_size(init_stages) + 1;
	*(void **)(&bp_set) = calloc(stages_total, sizeof(breakpoint_set_t));
	// -----------

	SetCurrentDirectory(game_dir);
	VLA_FREE(game_dir);
	VLA_FREE(exe_fn);
	json_decref(user_cfg);
	return thcrap_init_binary(0, nullptr);
}

int BP_init_next_stage(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	auto module = (HMODULE)json_object_get_immediate(bp_info, regs, "module");
	// ----------
	thcrap_init_binary(++stage_cur, &module);
	return 1;
}

int thcrap_init_binary(size_t stage_num, HMODULE *hModPtr)
{
	assert(bp_set);
	assert(stage_num < stages_total);

	if(stages_total >= 2) {
		log_printf(
			"Initialization stage %d...\n"
			"-------------------------\n",
			stage_num
		);
	}
	MessageBoxA(0, "", 0, 0);
	int ret = 0;
	json_t *run_cfg = runconfig_get();
	json_t *stage = thcrap_init_stage_data(stage_num);

	json_t *binhacks = json_object_get(stage, "binhacks");
	json_t *breakpoints = json_object_get(stage, "breakpoints");
	json_t *codecaves = json_object_get(stage, "codecaves");

	HMODULE hModFromStage = (HMODULE)json_object_get_immediate(
		stage, nullptr, "module"
	);
	HMODULE hMod = hModPtr ? *hModPtr : hModFromStage;

	if (json_is_object(codecaves)) {
		ret += codecaves_apply(codecaves, hMod);
	}

	ret += binhacks_apply(binhacks, hMod);
	// FIXME: this workaround is needed, because breakpoints don't check what they overwrite
	if (!(ret != 0 && stage_num == 0 && stages_total >= 2)){
		ret += breakpoints_apply(&bp_set[stage_num], breakpoints, hMod);
	}

	if(stages_total >= 2) {
		if(ret != 0 && stage_num == 0 && stages_total >= 2) {
			log_printf(
				"Failed. Jumping to last stage...\n"
				"-------------------------\n"
			);
			return thcrap_init_binary(stages_total - 1, nullptr);
		} else {
			log_printf("-----------------------\n");
		}
	}

	if(stage == run_cfg) {
		log_printf("---------------------------\n");
		log_printf("Complete run configuration:\n");
		log_printf("---------------------------\n");
		json_dump_log(run_cfg, JSON_INDENT(2));
		log_printf("---------------------------\n");
		mod_func_run_all("post_init", NULL);
	}
	return 0;
}

json_t* thcrap_init_stage_data(size_t stage_num)
{
	auto run_cfg = runconfig_get();
	auto init_stages = json_object_get(run_cfg, "init_stages");
	if(stage_num < json_array_size(init_stages)) {
		return json_array_get(init_stages, stage_num);
	}
	return run_cfg;
}

int InitDll(HMODULE hDll)
{

	w32u8_set_fallback_codepage(932);

	exception_init();
	// Needs to be at the lowest level
	win32_detour();
	detour_chain("kernel32.dll", 0,
		"ExitProcess", thcrap_ExitProcess,
		NULL
	);

	hThcrap = hDll;

	// Store the DLL's own directory to load plug-ins later
	dll_dir = (char*)malloc(MAX_PATH);
	GetCurrentDirectory(MAX_PATH, dll_dir);
	PathAddBackslashA(dll_dir);

	return 0;
}

void ExitDll(HMODULE hDll)
{
	// Yes, the main thread does not receive a DLL_THREAD_DETACH message
	mod_func_run_all("thread_exit", NULL);
	mod_func_run_all("exit", NULL);
	plugins_close();
	SAFE_FREE(bp_set);
	run_cfg = json_decref_safe(run_cfg);

	SAFE_FREE(dll_dir);
	detour_exit();
#if defined(_MSC_VER) && defined(_DEBUG)
	// Called in RawDllMain() in this configuration
#else
	log_exit();
#endif
}

VOID WINAPI thcrap_ExitProcess(UINT uExitCode)
{
	ExitDll(NULL);
	// The detour cache is already freed at this point, and this will
	// always be the final detour in the chain, so detour_next() doesn't
	// make any sense here (and would leak memory as well).
	ExitProcess(uExitCode);
}

BOOL APIENTRY DllMain(HMODULE hDll, DWORD ulReasonForCall, LPVOID lpReserved)
{
	switch(ulReasonForCall) {
		case DLL_PROCESS_ATTACH:
			InitDll(hDll);
			break;
		case DLL_PROCESS_DETACH:
			ExitDll(hDll);
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
