/**
  * Touhou Community Reliant Automatic Patcher
  * Loader
  *
  * ----
  *
  * Parse patch setup and launch game
  */

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#include <thcrap.h>
#include <string>
#include <vector>
#include <filesystem>
#include <thcrap_update_wrapper.h>

const char *EXE_HELP =
	"The executable can either be a game ID which is then looked up in "
	"games.js, or simply the relative or absolute path to an .exe file.\n"
	"It can either be given as a command line parameter, or through the "
	"run configuration as a \"game\" value for a game ID or an \"exe\" "
	"value for an .exe file path. If both are given, \"exe\" takes "
	"precedence.";

const char *game_missing = NULL;
size_t current_dir_len = 0;

bool update_finalize(std::vector<std::string>& logs);

const char* game_lookup(const json_t *games_js, const char *game, const char *base_dir)
{
	const json_t *game_path = json_object_get(games_js, game);
	if (!json_string_length(game_path)) {
		game_missing = game;
		return nullptr;
	}
	const char *game_path_str = json_string_value(game_path);
	if (PathIsRelativeA(game_path_str)) {
		char* ret = (char*)malloc(strlen(base_dir) + strlen(game_path_str) + 1);
		strcpy(ret, base_dir);
		PathAppendA(ret, game_path_str);
		return ret;
	}
	return game_path_str;
}

json_t *load_config_from_file(const char *rel_start, json_t *run_cfg, const char *config_path)
{
	std::string run_cfg_fn;

	if (PathIsRelativeU(config_path)) {
		if (strchr(config_path, '\\')) {
			run_cfg_fn = std::string(rel_start) + config_path;
		}
		else {
			run_cfg_fn = (std::filesystem::current_path() / "config" / config_path).u8string();
		}
	}
	else {
		run_cfg_fn = config_path;
	}

	log_printf("Loading run configuration %s... ", run_cfg_fn.c_str());
	json_t *new_run_cfg = json_load_file_report(run_cfg_fn.c_str());
	log_print(new_run_cfg != nullptr ? "found\n" : "not found\n");

	if (!run_cfg) {
		run_cfg = new_run_cfg;
	}
	else if (new_run_cfg) {
		run_cfg = json_object_merge(run_cfg, new_run_cfg);
	}
	json_array_append_new(json_object_get_create(run_cfg, "runcfg_fn", JSON_ARRAY), json_string(run_cfg_fn.c_str()));
	return run_cfg;
}

json_t *load_config_from_string(json_t *run_cfg, const char *config_string)
{
	log_printf("Loading run configuration from command-line... ");
	json_t *new_run_cfg = json5_loadb(config_string, strlen(config_string), nullptr);
	log_print(new_run_cfg != nullptr ? "success\n" : "error\n");

	if (!run_cfg) {
		run_cfg = new_run_cfg;
	}
	else if (new_run_cfg) {
		run_cfg = json_object_merge(run_cfg, new_run_cfg);
	}
	json_array_append_new(json_object_get_create(run_cfg, "runcfg_fn", JSON_ARRAY), json_string(
		(std::string("stdin:") + config_string).c_str()
	));
	return run_cfg;
}

char *find_exe_from_cfg(const char *rel_start, json_t *run_cfg, json_t *games_js)
{
	const char *new_exe_fn = NULL;
	char *cfg_exe_fn = NULL;

	new_exe_fn = json_object_get_string(run_cfg, "exe");
	if (!new_exe_fn) {
		const char *game = json_object_get_string(run_cfg, "game");
		if (game) {
			new_exe_fn = game_lookup(games_js, game, std::filesystem::current_path().u8string().c_str());
		}
	}
	if (new_exe_fn) {
		if (PathIsRelativeU(new_exe_fn)) {
			cfg_exe_fn = (char*)malloc(current_dir_len + strlen(new_exe_fn) + 2);
			strcpy(cfg_exe_fn, rel_start);
			PathAppendU(cfg_exe_fn, new_exe_fn);
		}
		else {
			cfg_exe_fn = (char*)malloc(strlen(new_exe_fn) + 1);
			strcpy(cfg_exe_fn, new_exe_fn);
		}
	}

	return cfg_exe_fn;
}

#include <win32_utf8/entry_winmain.c>

int TH_CDECL win32_utf8_main(int argc, const char *argv[])
{
	size_t rel_start_len = GetCurrentDirectoryU(0, NULL);
	VLA(char, rel_start, (rel_start_len + 1));
	GetCurrentDirectoryU(rel_start_len, rel_start);
	PathAddBackslashU(rel_start);

	char current_dir[MAX_PATH];
	GetModuleFileNameU(NULL, current_dir, MAX_PATH);
	PathRemoveFileSpecU(current_dir);
	PathAppendU(current_dir, "..");
	PathAddBackslashU(current_dir);
	SetCurrentDirectoryU(current_dir);

	size_t current_dir_len = strlen(current_dir);

	int ret;
	json_t *games_js = NULL;

	std::string run_cfg_fn;
	json_t *run_cfg = NULL;

	const char *game_id = nullptr;
	const char *cmd_exe_fn = NULL;
	char *cfg_exe_fn = NULL;
	const char *final_exe_fn = NULL;
	size_t run_cfg_fn_len = 0;
	

	// If thcrap just updated itself, finalize the update by moving things around if needed.
	// This can be done before parsing the command line.
	// We can't call log_init before this because we might move some log files, but we still want to log things,
	// so we'll buffer the logs somewhere and display them a bit later.
	std::vector<std::string> update_finalize_logs;
	bool update_finalize_ret = update_finalize(update_finalize_logs);

	globalconfig_init();
	log_init(globalconfig_get_boolean("console", false));

	for (auto& it : update_finalize_logs) {
		log_printf("%s\n", it.c_str());
	}
	if (!update_finalize_ret) {
		return 1;
	}

	if(argc < 2) {
		log_mboxf(NULL, MB_OK | MB_ICONINFORMATION,
			"This is the command-line loader component of the %s.\n"
			"\n"
			"You are probably looking for the configuration tool to set up shortcuts for the simple usage of the patcher - that would be thcrap_configure.\n"
			"\n"
			"If not, this is how to use the loader from the command line:\n"
			"\n"
			"\tthcrap_loader runconfig.js executable\n"
			"\n"
			"- The run configuration file must end in .js to be recognized as such.\n"
			"- %s\n"
			"- Also, later command-line parameters take priority over earlier ones.\n",
			PROJECT_NAME, EXE_HELP
		);
		ret = -1;
		goto end;
	}

	// Load games.js
	{
		size_t games_js_fn_len = GetCurrentDirectoryU(0, NULL) + 1 + strlen("config\\games.js") + 1;
		VLA(char, games_js_fn, games_js_fn_len);

		GetCurrentDirectoryU(games_js_fn_len, games_js_fn);
		PathAddBackslashA(games_js_fn);
		strcat(games_js_fn, "config\\games.js");
		games_js = json_load_file_report(games_js_fn);
		VLA_FREE(games_js_fn);
	}

	// Parse command line
	log_print("Parsing command line...\n");
	for(int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		const char *param_ext = PathFindExtensionA(arg);

		if (!stricmp(param_ext, ".js")) {
			run_cfg = load_config_from_file(rel_start, run_cfg, arg);
		}
		else if (arg[0] == '{') {
			run_cfg = load_config_from_string(run_cfg, arg);
		}
		else if(!stricmp(param_ext, ".exe")) {
			cmd_exe_fn = arg;
		}
		else {
			// Need to set game_missing even if games_js is null.
			cmd_exe_fn = game_lookup(games_js, arg, current_dir);
			game_id = arg;
		}
	}
	cfg_exe_fn = find_exe_from_cfg(rel_start, run_cfg, games_js);

	if(!run_cfg) {
		if(run_cfg_fn.empty()) {
			log_mbox(NULL, MB_OK | MB_ICONEXCLAMATION,
				"No run configuration .js file given.\n"
				"\n"
				"If you do not have one yet, use the "
				"thcrap_configure tool to create one.\n"
			);
			ret = -2;
		} else {
			log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
				"The run configuration file \"%s\" was not found.\n",
				run_cfg_fn.c_str()
			);

			ret = -4;
		}
		goto end;
	}
	// Command-line arguments take precedence over run configuration values
	final_exe_fn = cmd_exe_fn ? cmd_exe_fn : cfg_exe_fn;
	log_printf("Using exe %s\n", final_exe_fn);

	// Still none?
	if(!final_exe_fn) {
		if(game_missing || !games_js) {
			log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
				"The game ID \"%s\" is missing in games.js.",
				game_missing
			);
		} else {
			log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
				"No target executable given.\n\n%s", EXE_HELP
			);
		}
		ret = -3;
		goto end;
	}

	runconfig_load(run_cfg, RUNCONFIG_NO_BINHACKS);

	char *cmdline = NULL;
	if (const char *temp_cmdline = runconfig_cmdline_get()) {
		cmdline = strdup(temp_cmdline);
	}

	log_print("Command-line parsing finished\n");
	ret = loader_update_with_UI_wrapper(final_exe_fn, cmdline, game_id);
end:
	json_decref(games_js);
	json_decref(run_cfg);
	runconfig_free();
	globalconfig_release();
	return ret;
}
