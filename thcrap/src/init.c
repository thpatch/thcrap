/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * DLL and engine initialization.
  */

#include "thcrap.h"
#include "plugin.h"
#include "binhack.h"
#include "exception.h"
#include "sha256.h"
#include "bp_file.h"
#include "dialog.h"
#include "textdisp.h"
#include "win32_detour.h"

/// Static global variables
/// -----------------------
// Required to get the exported functions of thcrap.dll.
static HMODULE hThcrap = NULL;
static char *dll_dir = NULL;
static const char *update_url_message =
	"The new version can be found at\n"
	"\n"
	"\t";
static const char *mbox_copy_message =
	"\n"
	"\n"
	"(Press Ctrl+C to copy the text of this message box and the URL)";
/// -----------------------

json_t* identify_by_hash(const char *fn, size_t *file_size, json_t *versions)
{
	unsigned char *file_buffer = file_read(fn, file_size);
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

int IsLatestBuild(json_t *build_obj, json_t **latest, json_t *run_ver)
{
	json_t *json_latest = json_object_get(run_ver, "latest");
	size_t i;
	if(!json_is_string(build_obj) || !latest || !json_latest) {
		return -1;
	}
	json_flex_array_foreach(json_latest, i, *latest) {
		if(json_equal(build_obj, *latest)) {
			return 1;
		}
	}
	return 0;
}

json_t* identify(const char *exe_fn)
{
	size_t exe_size;
	json_t *run_ver = NULL;
	json_t *versions_js = stack_json_resolve("versions.js", NULL);
	json_t *game_obj = NULL;
	json_t *build_obj = NULL;
	json_t *variety_obj = NULL;
	const char *game = NULL;
	const char *build = NULL;
	const char *variety = NULL;

	// Result of the EXE identification (array)
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
	game = json_string_value(game_obj);
	build = json_string_value(build_obj);
	variety = json_string_value(variety_obj);

	if(!game || !build) {
		log_printf("Invalid version format!");
		goto end;
	}

	// Store build in the runconfig to be recalled later for
	// version-dependent patch file resolving. Needs be directly written to
	// run_cfg because we already require it down below to resolve ver_fn.
	json_object_set(run_cfg, "build", build_obj);

	log_printf("→ %s %s %s\n", game, build, variety);

	if(stricmp(PathFindExtensionA(game), ".js")) {
		size_t ver_fn_len = strlen(game) + 1 + strlen(".js") + 1;
		VLA(char, ver_fn, ver_fn_len);
		sprintf(ver_fn, "%s.js", game);
		run_ver = stack_json_resolve(ver_fn, NULL);
		VLA_FREE(ver_fn);
	} else {
		run_ver = stack_json_resolve(game, NULL);
	}

	// Ensure that we have a configuration with a "game" key
	if(!run_ver) {
		run_ver = json_object();
	}
	if(!json_object_get_string(run_ver, "game")) {
		json_object_set(run_ver, "game", game_obj);
	}

	// Pretty game title
	{
		const char *game_title = json_object_get_string(run_ver, "title");
		if(game_title) {
			game = game_title;
		}
	}

	if(size_cmp) {
		int ret = log_mboxf("Unknown version detected", MB_YESNO | MB_ICONQUESTION,
			"You have attached %s to an unknown game version.\n"
			"According to the file size, this is most likely\n"
			"\n"
			"\t%s %s %s\n"
			"\n"
			"but we haven't tested this exact variety yet and thus can't confirm that the patches will work.\n"
			"They might crash the game, damage your save files or cause even worse problems.\n"
			"\n"
			"Please send <%s> to\n"
			"\n"
			"\tsubmissions@thpatch.net\n"
			"\n"
			"We will take a look at it, and add support if possible.\n"
			"\n"
			"Apply patches for the identified game version regardless (on your own risk)?",
			PROJECT_NAME_SHORT(), game, build, variety, exe_fn
		);
		if(ret == IDNO) {
			run_ver = json_decref_safe(run_ver);
		}
	} else {
		// Old version nagbox
		json_t *latest = NULL;
		if(IsLatestBuild(build_obj, &latest, run_ver) == 0 && json_is_string(latest)) {
			const char *url_update = json_object_get_string(run_ver, "url_update");
			log_mboxf("Old version detected", MB_OK | MB_ICONINFORMATION,
				"You are running an old version of %s (%s).\n"
				"\n"
				"While %s is confirmed to work with this version, we recommend updating "
				"your game to the latest official version (%s).%s%s%s%s",
				game, build, PROJECT_NAME_SHORT(), json_string_value(latest),
				url_update ? "\n\n": "",
				url_update ? update_url_message : "",
				url_update ? url_update : "",
				url_update ? mbox_copy_message : ""
			);
		}
	}
end:
	json_decref(versions_js);
	return run_ver;
}

void thcrap_detour(HMODULE hProc)
{
	win32_detour(hProc);
	exception_detour(hProc);
	textdisp_detour(hProc);
	dialog_detour(hProc);
	strings_detour(hProc);
	inject_detour(hProc);
}

int thcrap_init(const char *run_cfg_fn)
{
	json_t *game_cfg = NULL;
	json_t *user_cfg = NULL;
	HMODULE hProc = GetModuleHandle(NULL);

	size_t exe_fn_len = GetModuleFileNameU(NULL, NULL, 0) + 1;
	size_t game_dir_len = GetCurrentDirectory(0, NULL) + 1;
	VLA(char, exe_fn, exe_fn_len);
	VLA(char, game_dir, game_dir_len);

	GetModuleFileNameU(NULL, exe_fn, exe_fn_len);
	GetCurrentDirectory(game_dir_len, game_dir);

	SetCurrentDirectory(dll_dir);

	user_cfg = json_load_file_report(run_cfg_fn);
	runconfig_set(user_cfg);

	{
		json_t *console_val = json_object_get(run_cfg, "console");
		log_init(json_is_true(console_val));
	}

	json_object_set_new(run_cfg, "run_cfg_fn", json_string(run_cfg_fn));
	log_printf("Run configuration file: %s\n\n", run_cfg_fn);

	thcrap_detour(hProc);

	log_printf("EXE file name: %s\n", exe_fn);

	game_cfg = identify(exe_fn);
	if(game_cfg) {
		json_object_merge(game_cfg, user_cfg);
		runconfig_set(game_cfg);
	}
	log_printf("Initializing patches...\n");
	{
		json_t *patches = json_object_get(run_cfg, "patches");
		size_t i;
		json_t *patch_info;
		DWORD min_build = 0;
		const char *url_engine = NULL;

		json_array_foreach(patches, i, patch_info) {
			DWORD cur_min_build;

			// Why, hello there, C89.
			int dummy = patch_rel_to_abs(patch_info, run_cfg_fn);

			json_array_set_new(patches, i, patch_init(patch_info));

			cur_min_build = json_object_get_hex(patch_info, "min_build");
			if(cur_min_build > min_build) {
				// ... OK, there *could* be the case where people stack patches from
				// different repositories which all require their own fork of the
				// patcher, and then one side updates their fork, causing this prompt,
				// and the users overwrite the modifications of another fork which they
				// need for running a certain patch configuration in the first place...
				// Let's just hope that it will never get that complicated.
				min_build = cur_min_build;
				url_engine = json_object_get_string(patch_info, "url_engine");
			}
		}
		if(min_build > PROJECT_VERSION()) {
			char format[11];
			str_hexdate_format(format, min_build);
			log_mboxf(NULL, MB_OK | MB_ICONINFORMATION,
				"A new version (%s) of the %s is available.\n"
				"\n"
				"This update contains new features and important bug fixes "
				"for your current patch configuration.%s%s%s%s",
				format, PROJECT_NAME(),
				url_engine ? "\n\n": "",
				url_engine ? update_url_message : "",
				url_engine ? url_engine : "",
				url_engine ? mbox_copy_message : ""
			);
		}
	}
	stack_show_missing();

	log_printf("Game directory: %s\n", game_dir);
	log_printf("Plug-in directory: %s\n", dll_dir);

	log_printf("\nInitializing plug-ins...\n");

	{
		// Copy format links from formats.js
		json_t *game_formats = json_object_get(run_cfg, "formats");
		if(game_formats) {
			json_t *formats_js = stack_json_resolve("formats.js", NULL);
			json_t *format_link;
			const char *key;
			json_object_foreach(game_formats, key, format_link) {
				json_t *format = json_object_get(formats_js, json_string_value(format_link));
				json_object_set_nocheck(game_formats, key, format);
			}
			json_decref(formats_js);
		}
	}

#ifdef HAVE_BP_FILE
	bp_file_init();
#endif
#ifdef HAVE_STRINGS
	strings_init();
#endif
#ifdef HAVE_TEXTDISP
	textdisp_init();
#endif
	plugins_load();

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
				SetCurrentDirectory(archive);
				plugins_load();
			}
		}
	}
	*/
	log_printf("---------------------------\n");
	log_printf("Complete run configuration:\n");
	log_printf("---------------------------\n");
	json_dump_log(run_cfg, JSON_INDENT(2));
	log_printf("---------------------------\n");
	{
		const char *key;
		json_t *val = NULL;
		json_t *run_funcs = json_object();

		GetExportedFunctions(run_funcs, hThcrap);
		json_object_foreach(json_object_get(run_cfg, "plugins"), key, val) {
			GetExportedFunctions(run_funcs, (HMODULE)json_integer_value(val));
		}

		json_object_set_new(run_cfg, "funcs", run_funcs);
		binhacks_apply(json_object_get(run_cfg, "binhacks"));
		breakpoints_apply(json_object_get(run_cfg, "breakpoints"));
	}
	SetCurrentDirectory(game_dir);
	VLA_FREE(game_dir);
	VLA_FREE(exe_fn);
	json_decref(user_cfg);
	json_decref(game_cfg);
	return 0;
}

int InitDll(HMODULE hDll)
{
	size_t dll_dir_len;

	w32u8_set_fallback_codepage(932);
	InitializeCriticalSection(&cs_file_access);
	exception_init();

#ifdef _WIN32
#ifdef _DEBUG
	// Activate memory leak debugging
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDOUT);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
#endif
#endif

	hThcrap = hDll;

	// Store the DLL's own directory to load plug-ins later
	dll_dir_len = GetCurrentDirectory(0, NULL) + 1;
	dll_dir = (char*)malloc(dll_dir_len * sizeof(char));
	GetCurrentDirectory(dll_dir_len, dll_dir);
	PathAddBackslashA(dll_dir);

	return 0;
}

void ExitDll(HMODULE hDll)
{
	plugins_close();
	breakpoints_remove();
	run_cfg = json_decref_safe(run_cfg);
	DeleteCriticalSection(&cs_file_access);

#ifdef HAVE_BP_FILE
	bp_file_exit();
#endif
#ifdef HAVE_STRINGS
	strings_exit();
#endif
	SAFE_FREE(dll_dir);
#ifdef _WIN32
#ifdef _DEBUG
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
	_CrtDumpMemoryLeaks();
#endif
#endif
	log_exit();
}

// Yes, this _has_ to be included in every project.
// Visual C++ won't use it when imported from a library
// and just defaults to msvcrt's one in this case.
BOOL APIENTRY DllMain(HMODULE hDll, DWORD ulReasonForCall, LPVOID lpReserved)
{
	switch(ulReasonForCall) {
		case DLL_PROCESS_ATTACH:
			InitDll(hDll);
			break;
		case DLL_PROCESS_DETACH:
			ExitDll(hDll);
			break;
	}
	return TRUE;
}
