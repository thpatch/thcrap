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
#include "sha256.h"
#include "bp_file.h"
#include "textdisp.h"
#include "win32_patch.h"

// Static global variables
// -----------------------
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
// -----------------------

json_t* identify_by_hash(const char *fn, size_t *file_size, json_t *versions)
{
	unsigned char *file_buffer;
	SHA256_CTX sha256_ctx;
	BYTE hash[32];
	char hash_str[65];
	int i;

	file_buffer = file_read(fn, file_size);
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
	char file_size_str[32];
	itoa(file_size, file_size_str, 10);
	return json_object_get(json_object_get(versions, "sizes"), file_size_str);
}

int IsLatestBuild(const char *build, const char **latest, json_t *run_ver)
{
	json_t *json_latest;
	size_t json_latest_count;
	size_t i;

	if(!build || !run_ver || !latest) {
		return -1;
	}
	
	json_latest = json_object_get(run_ver, "latest");
	json_latest_count = json_array_size(json_latest);

	if(json_latest_count == 0) {
		json_latest_count = 1;
	}
	for(i = 0; i < json_latest_count; i++) {
		if(json_is_array(json_latest)) {
			*latest = json_array_get_string(json_latest, i);
		} else {
			*latest = json_string_value(json_latest);
		}
		if(!strcmp(*latest, build)) {
			return 1;
		}
	}
	return 0;
}

json_t* identify(const char *exe_fn)
{
	size_t exe_size;
	json_t *run_ver = NULL;
	json_t *versions_js = NULL;
	const char *game = NULL;
	const char *build = NULL;
	const char *variety = NULL;
	
	// Result of the EXE identification (array)
	json_t *id_array = NULL;
	int size_cmp = 0;

	versions_js = stack_json_resolve("versions.js", NULL);
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

	if(json_array_size(id_array) >= 3) {
		game = json_array_get_string(id_array, 0);
		build = json_array_get_string(id_array, 1);
		variety = json_array_get_string(id_array, 2);
	}

	if(!game || !build) {
		log_printf("Invalid version format!");
		goto end;
	}
	{
		json_t *ver_json;

		// Version filename format is <game>.<version>.js
		size_t ver_fn_len = strlen(game) + 1 + strlen(build) + strlen(".js") + 1;
		VLA(char, ver_fn, ver_fn_len);

		log_printf("→ %s %s %s\n", game, build, variety);
			
		// Get the base game file
		if(stricmp(PathFindExtensionA(game), ".js")) {
			sprintf(ver_fn, "%s.js", game);
			run_ver = stack_json_resolve(ver_fn, NULL);
		} else {
			run_ver = stack_json_resolve(game, NULL);
		}
		if(!run_ver) {
			goto end;
		}
		
		sprintf(ver_fn, "%s.%s.js", game, build);

		// Merge!
		ver_json = stack_json_resolve(ver_fn, NULL);
		json_object_merge(run_ver, ver_json);
		json_decref(ver_json);
		VLA_FREE(ver_fn);
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
			json_decref(run_ver);
			run_ver = NULL;
		}
	} else {
		// Old version nagbox
		const char *latest = NULL;
		if(IsLatestBuild(build, &latest, run_ver) == 0) {
			const char *url_update = json_object_get_string(run_ver, "url_update");
			log_mboxf("Old version detected", MB_OK | MB_ICONINFORMATION, 
				"You are running an old version of %s (%s).\n"
				"\n"
				"While %s is confirmed to work with this version, we recommend updating "
				"your game to the latest official version (%s).%s%s%s%s",
				game, build, PROJECT_NAME_SHORT(), latest,
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

int thcrap_init(const char *setup_fn)
{
	char exe_fn[MAX_PATH * 4];
	json_t *run_ver = NULL;
	HMODULE hProc = GetModuleHandle(NULL);

	size_t game_dir_len = GetCurrentDirectory(0, NULL) + 1;
	VLA(char, game_dir, game_dir_len);
	GetCurrentDirectory(game_dir_len, game_dir);

	SetCurrentDirectory(dll_dir);

	run_cfg = json_load_file_report(setup_fn);

	{
		json_t *console_val = json_object_get(run_cfg, "console");
		log_init(json_is_true(console_val));
	}

	log_printf("Run configuration file: %s\n\n", setup_fn);

	win32_patch(hProc);
	if(!textdisp_init(hProc)) {
		textdisp_patch(hProc);
	}
	
	GetModuleFileName(NULL, exe_fn, MAX_PATH * 4);
	log_printf("EXE file name: %s\n", exe_fn);

	run_ver = identify(exe_fn);
	// Alright, smash our run configuration on top
	if(run_ver) {
		json_object_merge(run_ver, run_cfg);
		json_decref(run_cfg);
		run_cfg = run_ver;
	}

	{
		// Copy format links from formats.js
		json_t *game_formats = json_object_get(run_cfg, "formats");
		if(game_formats) {
			json_t *formats_js = stack_json_resolve("formats.js", NULL);
			json_t *format_link;
			const char *key;
			json_object_foreach(game_formats, key, format_link) {
				if(json_is_string(format_link)) {
					json_t *format = json_object_get(formats_js, json_string_value(format_link));
					if(format) {
						json_object_set_new_nocheck(game_formats, key, json_deep_copy(format));
					}
				}
			}
			json_decref(formats_js);
		}
	}
	
	log_printf("Initializing patches...\n");
	{
		json_t *patches = json_object_get(run_cfg, "patches");
		size_t i;
		json_t *patch_info;
		DWORD min_build = 0;
		char *url_engine = NULL;
		json_t *rem_arcs = NULL;
		size_t rem_arcs_str_len = 0;

		json_array_foreach(patches, i, patch_info) {
			json_t *patch_js;
			DWORD cur_min_build;

			// Check archive presence
			json_t *archive_obj = json_object_get(patch_info, "archive");
			const char *archive = json_string_value(archive_obj);
			if(archive) {
				if(!PathFileExists(archive)) {
					if(!rem_arcs) {
						rem_arcs = json_array();
					}
					json_array_append(rem_arcs, archive_obj);
					rem_arcs_str_len += 1 + strlen(archive) + 1;
				}
			} else {
				// No archive?!?
				// Is this an error? I'm not sure
			}

			patch_js = patch_json_load(patch_info, "patch.js", NULL);

			cur_min_build = json_object_get_hex(patch_js, "min_build");
			if(cur_min_build > min_build) {
				// ... OK, there *could* be the case where people stack patches from
				// different originating servers which all have their own fork of the
				// patcher, and then one side updates their fork, causing this prompt,
				// and the users overwrite the modifications of another fork which they
				// need for running a certain patch configuration in the first place...
				// Let's just hope that it will never get that complicated.
				min_build = cur_min_build;
				SAFE_FREE(url_engine);
				url_engine = strdup(json_object_get_string(patch_js, "url_engine"));
			}
			patch_fonts_load(patch_info, patch_js);
			json_decref(patch_js);
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
			SAFE_FREE(url_engine);
		}
		if(json_array_size(rem_arcs) > 0) {
			VLA(char, rem_arcs_str, rem_arcs_str_len);
			size_t i;
			json_t *archive_obj;

			rem_arcs_str[0] = 0;
			json_array_foreach(rem_arcs, i, archive_obj) {
				strcat(rem_arcs_str, "\t");
				strcat(rem_arcs_str, json_string_value(archive_obj));
				strcat(rem_arcs_str, "\n");
			}
			log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
				"Some patches in your configuration could not be found:\n"
				"\n"
				"%s"
				"\n"
				"Please reconfigure your patch stack - either by running the configuration tool, "
				"or by simply editing your run configuration file (%s).",
				rem_arcs_str, setup_fn
			);
			json_decref(rem_arcs);
			VLA_FREE(rem_arcs_str);
		}
	}
	
	log_printf("Game directory: %s\n", game_dir);
	log_printf("Plug-in directory: %s\n", dll_dir);

	log_printf("\nInitializing plug-ins...\n");

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
	{
		const char *key;
		json_t *val = NULL;
		json_t *run_funcs = json_object();

		// Print this separately from the run configuration
		
		log_printf("Functions available to binary hacks:\n");
		log_printf("------------------------------------\n");
		
		GetExportedFunctions(run_funcs, hThcrap);
		json_object_foreach(json_object_get(run_cfg, "plugins"), key, val) {
			log_printf("\n%s:\n\n", key);
			GetExportedFunctions(run_funcs, (HMODULE)json_integer_value(val));
		}
		
		log_printf("------------------------------------\n");
		log_printf("\n");
		
		binhacks_apply(json_object_get(run_cfg, "binhacks"), run_funcs);
		breakpoints_apply();

		// -----------------
		log_printf("---------------------------\n");
		log_printf("Complete run configuration:\n");
		log_printf("---------------------------\n");
		json_dump_log(run_cfg, JSON_INDENT(2));
		log_printf("---------------------------\n");
		SetCurrentDirectory(game_dir);

		json_object_set_new(run_ver, "funcs", run_funcs);
	}
	VLA_FREE(game_dir);
	return 0;
}

int InitDll(HMODULE hDll)
{	
	size_t dll_dir_len;

	w32u8_set_fallback_codepage(932);

	InitializeCriticalSection(&cs_file_access);
	
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

	// Free our global variables
	breakpoints_remove();

	json_decref(run_cfg);

	DeleteCriticalSection(&cs_file_access);

	SAFE_FREE(dll_dir);
#ifdef _WIN32
#ifdef _DEBUG
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
