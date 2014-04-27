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
	runconfig_set(run_cfg);
	json_object_set_new(run_cfg, "run_cfg_fn", json_string(run_cfg_fn));
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
	{
		STRLEN_DEC(final_exe_fn);
		VLA(char, game_dir, final_exe_fn_len);
		VLA(char, final_exe_fn_local, final_exe_fn_len);
		STARTUPINFOA si = {0};
		PROCESS_INFORMATION pi = {0};

		strcpy(final_exe_fn_local, final_exe_fn);
		str_slash_normalize_win(final_exe_fn_local);

		strcpy(game_dir, final_exe_fn);
		PathRemoveFileSpec(game_dir);

		/**
		  * Initialize the detours for the injection module.
		  *
		  * This is an unfortunate drawback introduced by detour chaining.
		  * Without any detours being set up, detour_next() doesn't have any
		  * chain to execute (and, in fact, doesn't even know its own position).
		  * Thus, it can only fall back to the original function, CreateProcessA(),
		  * which in turn means that we lose Unicode support in this instance.
		  *
		  * We work around this by hardcoding calls to the necessary detour setup
		  * functions to make the function run as expected.
		  * Sure, the alternative would be to set up the entire engine with all
		  * plug-ins and modules. While it would indeed be nice to allow those to
		  * control initial startup, it really shouldn't be necessary for now - and
		  * it really does run way too much unnecessary code for my taste.
		  */
		win32_detour();
		inject_mod_detour();

		ret = detour_next("kernel32.dll", "CreateProcessA", NULL, 10,
			final_exe_fn_local, game_dir, NULL, NULL, TRUE, 0, NULL, game_dir, &si, &pi
		);
		if(!ret) {
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
