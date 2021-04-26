/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Run configuration.
  */

#include "thcrap.h"

struct stage_t
{
	HMODULE module;
	std::vector<binhack_t> binhacks;
	std::vector<codecave_t> codecaves;
	std::vector<breakpoint_t> breakpoints;
};

struct runconfig_t
{
	// Run configuration in json format (needed for extensions by plugins)
	json_t *json = nullptr;
	// True if the console should be enabled (from runcfg)
	bool console;
	// Thcrap directory (from runtime)
	std::string thcrap_dir;
	// Run configuration path (from runtime)
	std::string runcfg_fn;
	// Game ID, for example "th06" (from game_id.js)
	std::string game;
	// Game build, for example "v1.00a" (from game_id.js)
	std::string build;
	// Original game title, for example "東方紅魔郷　～ the Embodiment of Scarlet Devil" (from game_id.js)
	std::string title;
	// URL to download the last game update (from game_id.js)
	std::string update_url;
	// Array of latest builds. For example, [ "v0.13", "v1.02h" ] (from game_id.js)
	std::vector<std::string> latest;
	// dat dump path if dat dump is enabled, empty otherwise (from runcfg)
	std::string dat_dump;
	// Hackpoints (breakpoints, codecaves and binhacks) for stages (from game_id.js and game_id.build.js).
	// The global hackpoints (those not in a stage) are put in the last stage.
	std::vector<stage_t> stages;
	// Boolean flag that tells the binhack parser if it should show a message box, should it fail to find a function
	bool msgbox_invalid_func;
};

static runconfig_t run_cfg;

static void runconfig_stage_load(json_t *stage_json)
{
	stage_t stage;
	const char *key;
	json_t *value;

	stage.module = (HMODULE)json_object_get_eval_int_default(stage_json, "module", NULL, JEVAL_STRICT | JEVAL_NO_EXPRS);

	json_t *options = json_object_get(stage_json, "options");
	if (options) {
		patch_opts_from_json(options);
	}

	json_t *binhacks = json_object_get(stage_json, "binhacks");
	json_object_foreach(binhacks, key, value) {
		binhack_t binhack;
		if (!binhack_from_json(key, value, &binhack)) {
			continue;
		}
		stage.binhacks.push_back(binhack);
	}

	json_t *codecaves = json_object_get(stage_json, "codecaves");
	json_object_foreach(codecaves, key, value) {
		if (strcmp(key, "protection") == 0) {
			continue;
		}

		if (strchr(key, '+')) {
			log_printf("Codecave %s contains illegal character +", key);
			continue;
		}

		codecave_t codecave;
		if (!codecave_from_json(key, value, &codecave)) {
			continue;
		}
		stage.codecaves.push_back(codecave);
	}

	json_t *breakpoints = json_object_get(stage_json, "breakpoints");
	json_object_foreach(breakpoints, key, value) {

		breakpoint_t breakpoint;
		if (!breakpoint_from_json(key, value, &breakpoint)) {
			continue;
		}
		stage.breakpoints.push_back(breakpoint);
	}

	run_cfg.stages.push_back(stage);
}

void runconfig_load(json_t *file, int flags)
{
	json_t *value;
	bool can_overwrite = !(flags & RUNCONFIG_NO_OVERWRITE);
	bool load_binhacks = !(flags & RUNCONFIG_NO_BINHACKS);

	if (!run_cfg.json) {
		run_cfg.json = json_object();
	}
	if (can_overwrite) {
		json_object_update_recursive(run_cfg.json, file);
	}
	else {
		// The JSON values already in run_cfg.json should be applied
		// over the new ones in file.
		json_object_update_missing(run_cfg.json, file);
	}

	auto set_string_if_exist = [file, can_overwrite](const char* key, std::string& out) {
		json_t *value = json_object_get(file, key);
		if (value && (out.empty() || can_overwrite)) {
			out = json_string_value(value);
		}
	};

	set_string_if_exist("game",  run_cfg.game);
	set_string_if_exist("build", run_cfg.build);
	set_string_if_exist("title", run_cfg.title);
	set_string_if_exist("url_update", run_cfg.update_url);

	value = json_object_get(file, "console");
	if (value) {
		run_cfg.console = json_is_true(value);
	}
	run_cfg.msgbox_invalid_func = json_is_true(json_object_get(run_cfg.json, "msgbox_invalid_func"));
	value = json_object_get(file, "dat_dump");
	if (value && (run_cfg.dat_dump.empty() || can_overwrite)) {
		if (json_is_string(value)) {
			run_cfg.dat_dump = json_string_value(value);
		}
		else if (!json_is_false(value)) {
			run_cfg.dat_dump = "dat";
		}
		else {
			run_cfg.dat_dump = "";
		}
	}

	value = json_object_get(file, "latest");
	if (value && (run_cfg.latest.empty() || can_overwrite)) {
		size_t i;
		json_t *latest_entry;

		run_cfg.latest.clear();
		json_flex_array_foreach(value, i, latest_entry) {
			if (json_is_string(latest_entry)) {
				run_cfg.latest.push_back(json_string_value(latest_entry));
			}
		}
	}

	value = json_object_get(file, "patches");
	if (value) {
		size_t i;
		json_t *patch;
		json_array_foreach(value, i, patch) {
			stack_add_patch_from_json(patch);
		}
	}

	value = json_object_get(file, "detours");
	if (value) {
		const char* dll_name;
		json_t* detours;
		json_object_foreach(value, dll_name, detours) {
			if (!detours) continue;
			switch (json_typeof(detours)) {
				case JSON_NULL:
					detour_disable(dll_name, NULL);
					break;
				case JSON_OBJECT: {
					const char* func_name;
					json_t* func_json;
					json_object_foreach(detours, func_name, func_json) {
						if (json_is_null(func_json)) {
							detour_disable(dll_name, func_name);
						}
					}
					break;
				}
			}
		}
	}

	if (load_binhacks) {
		json_t *stages = json_object_get(file, "init_stages");
		if ((can_overwrite || run_cfg.stages.empty()) &&
			(stages || json_object_get(file, "binhacks") || json_object_get(file, "codecaves") || json_object_get(file, "breakpoints")) || json_object_get(file, "options")
			) {
			run_cfg.stages.clear();
			size_t i;
			json_flex_array_foreach(stages, i, value) {
				runconfig_stage_load(value);
			};
			runconfig_stage_load(file);
		}
	}
}

void runconfig_load_from_file(const char *path)
{
	json_t *new_cfg = json_load_file_report(path);
	runconfig_load(new_cfg, 0);
	runconfig_runcfg_fn_set(path);
	json_decref(new_cfg);
}

void runconfig_print()
{
	log_printf(
		"---------------------------\n"
		"Complete run configuration:\n"
		"---------------------------\n"
	);
	log_printf("  console: %s\n",      run_cfg.console ? "true" : "false");
	log_printf("  thcrap dir: '%s'\n", run_cfg.thcrap_dir.c_str());
	log_printf("  runcfg fn: '%s'\n",  run_cfg.runcfg_fn.c_str());
	log_printf("  game id: '%s'\n",    run_cfg.game.c_str());
	log_printf("  build id: '%s'\n",   run_cfg.build.c_str());
	log_printf("  game title: '%s'\n", run_cfg.title.c_str());
	log_printf("  update URL: '%s'\n", run_cfg.update_url.c_str());
	log_printf("  latest:", run_cfg);
	for (const std::string& it : run_cfg.latest) {
		log_printf(" '%s'", it.c_str());
	}
	log_print("\n");
	log_printf("  dat_dump: '%s'\n", run_cfg.dat_dump.c_str());
	log_printf(
		"---------------------------\n"
		"Patch stack:\n"
		"---------------------------\n"
	);
	stack_print();
	log_printf(
		"---------------------------\n"
		"Run configuration JSON:\n"
		"---------------------------\n"
	);
	json_dump_log(run_cfg.json, JSON_INDENT(2));
	log_printf(
		"---------------------------\n"
	);
}

void runconfig_free()
{
	stack_free();
	json_decref_safe(run_cfg.json);
	run_cfg.json = nullptr;
	run_cfg.console = false;
	run_cfg.thcrap_dir.clear();
	run_cfg.runcfg_fn.clear();
	run_cfg.game.clear();
	run_cfg.build.clear();
	run_cfg.title.clear();
	run_cfg.update_url.clear();
	run_cfg.latest.clear();
	run_cfg.dat_dump.clear();
	for (auto& stage : run_cfg.stages) {
		for (auto& binhack : stage.binhacks) {
			free((void*)binhack.name);
			free((void*)binhack.title);
			free((void*)binhack.code);
			free((void*)binhack.expected);
			for (size_t i = 0; binhack.addr[i].type != END_ADDR; ++i) {
				if (binhack.addr[i].str) {
					free((void*)binhack.addr[i].str);
				}
				if (binhack.addr[i].binhack_source) {
					free(binhack.addr[i].binhack_source);
				}
			}
			free(binhack.addr);
		}
		stage.binhacks.clear();
		for (auto& codecave : stage.codecaves) {
			free((void*)codecave.name);
			free((void*)codecave.code);
		}
		stage.codecaves.clear();
		for (auto& breakpoint : stage.breakpoints) {
			free((void*)breakpoint.name);
			for (size_t i = 0; breakpoint.addr[i].type != END_ADDR; ++i) {
				if (breakpoint.addr[i].str) {
					free((void*)breakpoint.addr[i].str);
				}
			}
			free(breakpoint.addr);
			json_decref(breakpoint.json_obj);
		}
		stage.breakpoints.clear();
	}
	run_cfg.stages.clear();
}

const json_t *runconfig_json_get()
{
	return run_cfg.json;
}

bool runconfig_console_get()
{
	return run_cfg.console;
}

const char *runconfig_thcrap_dir_get()
{
	return run_cfg.thcrap_dir.empty() == false ? run_cfg.thcrap_dir.c_str() : nullptr;
}

void runconfig_thcrap_dir_set(const char *thcrap_dir)
{
	if (thcrap_dir != nullptr) {
		run_cfg.thcrap_dir = thcrap_dir;
	}
	else {
		run_cfg.thcrap_dir.clear();
	}
}

const char *runconfig_runcfg_fn_get()
{
	return run_cfg.runcfg_fn.empty() == false ? run_cfg.runcfg_fn.c_str() : nullptr;
}

void runconfig_runcfg_fn_set(const char *runcfg_fn)
{
	if (runcfg_fn != nullptr) {
		run_cfg.runcfg_fn = runcfg_fn;
	}
	else {
		run_cfg.runcfg_fn.clear();
	}
}

const char *runconfig_game_get()
{
	return run_cfg.game.empty() == false ? run_cfg.game.c_str() : nullptr;
}

const char *runconfig_build_get()
{
	return run_cfg.build.empty() == false ? run_cfg.build.c_str() : nullptr;
}

void runconfig_build_set(const char *build)
{
	if (build != nullptr) {
		run_cfg.build = build;
	}
	else {
		run_cfg.build.clear();
	}
}

const char *runconfig_title_get()
{
	// Try to get a translated title
	if (!run_cfg.game.empty()) {
		const json_t *title = strings_get(run_cfg.game.c_str());
		if (title) {
			return json_string_value(title);
		}
	}

	// Get the title from [game_id].js
	if (!run_cfg.title.empty()) {
		return run_cfg.title.c_str();
	}

	// Fallback to the game id
	if (!run_cfg.game.empty()) {
		return run_cfg.game.c_str();
	}

	// Nothing worked
	return nullptr;
}

const char *runconfig_update_url_get()
{
	return run_cfg.update_url.empty() == false ? run_cfg.update_url.c_str() : nullptr;
}

const char *runconfig_dat_dump_get()
{
	return run_cfg.dat_dump.empty() == false ? run_cfg.dat_dump.c_str() : nullptr;
}

bool runconfig_latest_check()
{
	if (run_cfg.latest.empty()) {
		return true;
	}

	for (const std::string& it : run_cfg.latest) {
		if (run_cfg.build == it) {
			return true;
		}
	}

	return false;
}

const char *runconfig_latest_get()
{
	if (run_cfg.latest.empty()) {
		return nullptr;
	}
	return run_cfg.latest.back().c_str();
}

bool runconfig_msgbox_invalid_func() {
	return run_cfg.msgbox_invalid_func;
}

size_t runconfig_stage_count()
{
	return run_cfg.stages.size();
}

bool runconfig_stage_apply(size_t stage_num, int flags, HMODULE module)
{
	assert(stage_num < run_cfg.stages.size());
	stage_t& stage = run_cfg.stages[stage_num];
	size_t failed = 0;

	if (flags & RUNCFG_STAGE_USE_MODULE) {
		module = stage.module;
	}

	failed += codecaves_apply(stage.codecaves.data(), stage.codecaves.size());
	failed += binhacks_apply(stage.binhacks.data(), stage.binhacks.size(), module);
	if (!(flags & RUNCFG_STAGE_SKIP_BREAKPOINTS)) {
		const size_t breakpoint_count = stage.breakpoints.size();
		if (stage_num == 0 && run_cfg.stages.size() > 1) {
			// Explicitly check the number of breakpoints when using
			// multiple init stages so that the last stage will still
			// be loaded even when all first stage breakpoints were
			// ignored or failed during initial parsing.
			// 
			// FIXME: testing failed is needed because breakpoints don't check what they overwrite
			if (failed > 0 || breakpoint_count == 0) {
				return false;
			}
		}
		failed += breakpoints_apply(stage.breakpoints.data(), breakpoint_count, module);
	}

	return failed == 0;
}
