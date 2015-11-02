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

const char *EXE_HELP =
	"The executable can either be a game ID which is then looked up in "
	"games.js, or simply the relative or absolute path to an .exe file.\n"
	"It can either be given as a command line parameter, or through the "
	"run configuration as a \"game\" value for a game ID or an \"exe\" "
	"value for an .exe file path. If both are given, \"exe\" takes "
	"precedence.";

const char *game_missing = NULL;

const char* game_lookup(const json_t *games_js, const char *game)
{
	const json_t *ret = json_object_get(games_js, game);
	if(!json_string_length(ret)) {
		game_missing = game;
	}
	return json_string_value(ret);
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	int ret;
	json_t *args = NULL;
	json_t *games_js = NULL;

	int run_cfg_seen = 0;
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
			"Sinon, Voici comment utiliser le loader en ligne de commande :\n"
			"\n"
			"\tthcrap_loader runconfig.js executable\n"
			"\n"
			"- Le fichier de configuration doit se terminer par .js\n"
			"- %s\n"
			"- De plus, les derniers paramètres des lignes de commande ont priorité sur les premiers\n",
			PROJECT_NAME(), EXE_HELP
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

			run_cfg_seen = 1;
			// Sorry guys, no run configuration stacking yet
			if(json_is_object(run_cfg)) {
				json_decref(run_cfg);
			}
			run_cfg_fn = arg;
			run_cfg = json_load_file_report(run_cfg_fn);

			new_exe_fn = json_object_get_string(run_cfg, "exe");
			if(!new_exe_fn) {
				const char *game = json_object_get_string(run_cfg, "game");
				if(game) {
					new_exe_fn = game_lookup(games_js, game);
				}
			}
			if(new_exe_fn) {
				cfg_exe_fn = new_exe_fn;
			}
		} else if(!stricmp(param_ext, ".exe")) {
			cmd_exe_fn = arg;
		} else {
			// Need to set game_missing even if games_js is null.
			cmd_exe_fn = game_lookup(games_js, arg);
		}
	}

	if(!run_cfg) {
		if(!run_cfg_seen) {
			log_mbox(NULL, MB_OK | MB_ICONEXCLAMATION,
				"Aucun fichier de configuration valide donné!\n"
				"\n"
				"Si vous n'en avez pas encore, utilisez l'outil thcrap_configure pour en créer un.\n"
			);
		}
		ret = -2;
		goto end;
	}
	// Command-line arguments take precedence over run configuration values
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
		if(game_missing || !games_js) {
			log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
				"games.js ne contient pas l'identifiant de jeu \"%s\".",
				game_missing
			);
		} else {
			log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
				"Aucun exécutable cible n'a été donné.\n\n%s", EXE_HELP
			);
		}
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
