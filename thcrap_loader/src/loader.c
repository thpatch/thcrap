/**
  * Touhou Community Reliant Automatic Patcher
  * Loader
  *
  * ----
  *
  * Parse patch setup and launch game
  */

#include <thcrap.h>
#include <win32_detour.h>

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	int ret;
	json_t *args = NULL;
	json_t *games_js = NULL;

	const char *run_cfg_fn = NULL;
	json_t *run_cfg = NULL;

	const char *cmd_exe_fn = NULL;
	const char *cfg_exe_fn = NULL;
	const char *final_exe_fn = NULL;

	size_t i;

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

	args = json_array_from_wchar_array(__argc, __wargv);

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
	for(i = 1; i < json_array_size(args); i++) {
		const char *arg = json_array_get_string(args, i);
		const char *param_ext = PathFindExtensionA(arg);

		if(!stricmp(param_ext, ".js")) {
			const char *new_exe_fn = NULL;

			// Sorry guys, no run configuration stacking yet
			if(json_is_object(run_cfg)) {
				json_decref(run_cfg);
			}
			run_cfg_fn = arg;
			run_cfg = json_load_file_report(run_cfg_fn);

			new_exe_fn = json_object_get_string(run_cfg, "exe");
			if(!new_exe_fn) {
				const char *game = json_object_get_string(run_cfg, "game");
				new_exe_fn = json_object_get_string(games_js, game);
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
	for(i = 1; i < json_array_size(args); i++)
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
	ret = thcrap_inject_into_new(final_exe_fn, NULL, run_cfg_fn);
end:
	json_decref(games_js);
	json_decref(run_cfg);
	json_decref(args);
	return ret;
}
