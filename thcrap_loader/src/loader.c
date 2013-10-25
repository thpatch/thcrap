/**
  * Touhou Community Reliant Automatic Patcher
  * Loader
  *
  * ----
  *
  * Parse patch setup and launch game
  */

#include <thcrap.h>

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
			"Ceci est le composant de chargement par lignes de commande de %s.\n"
			"\n"
			"Vous recherchez probablement l'outil de configuration pour mettre en place des raccourcis pour l'usage simplifié du patcheur - il s'agit dans ce cas de thcrap_configure.\n"
			"\n"
			"Sinon, Voici comment utiliser le loader directement:\n"
			"\n"
			"\tthcrap_loader runconfig.js [game_id] [game.exe]\n"
			"\n"
			"- le fichier de configuration doit se terminer par .js\n"
			"- Si [game_id] est donné, l'emplacement de l'exécutable du jeu est lu depuis games.js\n"
			"- les derniers paramètres des lignes de commande ont priorité sur les premiers\n",
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
			"Aucun fichier de configuration valide donné!\n"
			"\n"
			"Si vous n'en avez pas encore, utilisez l'outil thcrap_configure pour en créer un.\n"
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
			"Aucun fichier de jeu exécutable donné!\n"
			"\n"
			"Celui-ci peut-être donné en argument de ligne de commande"
			"(Ajoutez simplement quelque chose se terminant par .exe), "
			"ou en tant que \"exe\" dans votre configuration de lancement (%s).\n",
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

		ret = inject_CreateProcessU(
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
				"Echec du lancement de %s: %s",
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
