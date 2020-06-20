/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Run configuration.
  */

#include "thcrap.h"
#include <string>
#include <vector>

struct stage_t
{
	HMODULE module;
	std::vector<binhack_t> binhacks;
	std::vector<codecave_t> codecaves;
	DWORD codecaves_protection;
	std::vector<breakpoint_local_t> breakpoints;
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
};

static runconfig_t run_cfg;

static void runconfig_stage_load_breakpoints(json_t *breakpoints, stage_t& stage)
{
	const char *key;
	json_t *breakpoint_entry;
	json_object_foreach(breakpoints, key, breakpoint_entry) {
		if (!json_is_object(breakpoint_entry)) {
			log_printf("breakpoint %s: not an object\n", key);
			continue;
		}

		json_t *addr_array = json_object_get(breakpoint_entry, "addr");
		size_t cavesize = json_object_get_hex(breakpoint_entry, "cavesize");
		bool ignore = json_is_true(json_object_get(breakpoint_entry, "ignore"));

		if (ignore) {
			log_printf("breakpoint %s: ignored\n", key);
			continue;
		}
		if (!cavesize) {
			log_printf("breakpoint %s: no cavesize specified\n", key);
			continue;
		}
		if (json_flex_array_size(addr_array) == 0) {
			// Ignore binhacks with missing addr field.
			// It usually means the breakpoint doesn't apply for this game or game version.
			continue;
		}

		size_t i;
		json_t *addr;
		json_flex_array_foreach(addr_array, i, addr) {
			if (json_is_string(addr)) {
				breakpoint_local_t bp;
				bp.name = strdup(key);
				bp.addr_str = strdup(json_string_value(addr));
				bp.cavesize = cavesize;
				bp.json_obj = json_incref(breakpoint_entry);
				bp.addr = nullptr;
				bp.func = nullptr;
				bp.cave = nullptr;
				stage.breakpoints.push_back(bp);
			}
		}
	}
}

static void runconfig_stage_load(json_t *stage_json)
{
	stage_t stage;
	const char *key;
	json_t *value;

	json_t *module = json_object_get(stage_json, "module");
	if (json_is_integer(module)) {
		stage.module = (HMODULE)json_integer_value(module);
	}
	else {
		stage.module = nullptr;
	}

	json_t *binhacks = json_object_get(stage_json, "binhacks");
	json_object_foreach(binhacks, key, value) {
		binhack_t binhack;
		if (binhack_from_json(key, value, &binhack)) {
			stage.binhacks.push_back(binhack);
		}
	}

	stage.codecaves_protection = 0;
	json_t *codecaves = json_object_get(stage_json, "codecaves");
	json_object_foreach(codecaves, key, value) {
		if (strcmp(key, "protection") == 0) {
			stage.codecaves_protection = json_hex_value(value);
			continue;
		}

		if (!json_is_string(value)) {
			// Don't print an error, this can be used for comments
			continue;
		}

		codecave_t codecave;
		codecave.name = strdup(key);
		codecave.code = strdup(json_string_value(value));
		stage.codecaves.push_back(codecave);
	}

	json_t *breakpoints = json_object_get(stage_json, "breakpoints");
	runconfig_stage_load_breakpoints(breakpoints, stage);

	run_cfg.stages.push_back(stage);
}

void runconfig_load(json_t *file, int flags)
{
	json_t *value;
	bool can_overwrite = (flags & RUNCONFIG_NO_OVERWRITE) == 0;

	if (!run_cfg.json) {
		run_cfg.json = json_object();
	}
	if (can_overwrite) {
		json_object_merge(run_cfg.json, file);
	}
	else {
		// The JSON values already in run_cfg.json should be applied
		// over the new ones in file.
		json_t *tmp = json_deep_copy(file);
		json_object_merge(tmp, run_cfg.json);
		json_decref(run_cfg.json);
		run_cfg.json = tmp;
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

	value = json_object_get(file, "console");
	if (value) {
		run_cfg.console = json_is_true(value);
	}

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

	json_t *stages = json_object_get(file, "init_stages");
	if ((stages || json_object_get(file, "binhacks") || json_object_get(file, "codecaves") || json_object_get(file, "breakpoints")) &&
		(run_cfg.stages.empty() || can_overwrite)) {
		run_cfg.stages.clear();
		size_t i;
		json_flex_array_foreach(stages, i, value) {
			runconfig_stage_load(value);
		};
		runconfig_stage_load(file);
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
	log_printf("---------------------------\n");
	log_printf("Complete run configuration:\n");
	log_printf("---------------------------\n");
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
	log_printf("---------------------------\n");
	log_printf("Patch stack:\n");
	log_printf("---------------------------\n");
	stack_print();
	log_printf("---------------------------\n");
	log_printf("Run configuration JSON:\n");
	log_printf("---------------------------\n");
	json_dump_log(run_cfg.json, JSON_INDENT(2));
	log_printf("---------------------------\n");
}

void runconfig_free()
{
	stack_free();
	json_decref(run_cfg.json);
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
			free(binhack.name);
			free(binhack.title);
			free(binhack.code);
			free(binhack.expected);
			for (size_t i = 0; binhack.addr[i]; i++) {
				free(binhack.addr[i]);
			}
			free(binhack.addr);
		}
		stage.binhacks.clear();
		for (auto& codecave : stage.codecaves) {
			free(codecave.name);
			free(codecave.code);
		}
		stage.codecaves.clear();
		for (auto& breakpoint : stage.breakpoints) {
			free(breakpoint.name);
			free(breakpoint.addr_str);
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
	return run_cfg.thcrap_dir.c_str();
}

void runconfig_thcrap_dir_set(const char *thcrap_dir)
{
	run_cfg.thcrap_dir = thcrap_dir;
}

const char *runconfig_runcfg_fn_get()
{
	return run_cfg.runcfg_fn.c_str();
}

void runconfig_runcfg_fn_set(const char *runcfg_fn)
{
	run_cfg.runcfg_fn = runcfg_fn;
}

const char *runconfig_game_get()
{
	return run_cfg.game.c_str();
}

const char *runconfig_build_get()
{
	return run_cfg.build.c_str();
}

void runconfig_build_set(const char *build)
{
	run_cfg.build = build;
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
	return run_cfg.update_url.c_str();
}

const char *runconfig_dat_dump_get()
{
	return run_cfg.dat_dump.c_str();
}

bool runconfig_latest_check()
{
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

size_t runconfig_stage_count()
{
	return run_cfg.stages.size();
}

bool runconfig_stage_apply(size_t stage_num, int flags, HMODULE module)
{
	assert(stage_num < run_cfg.stages.size());
	stage_t& stage = run_cfg.stages[stage_num];
	HMODULE hMod;
	int ret = 0;

	if (flags & RUNCFG_STAGE_USE_MODULE) {
		hMod = module;
	}
	else {
		hMod = stage.module;
	}

	ret += codecaves_apply(stage.codecaves.data(), stage.codecaves.size(), stage.codecaves_protection);
	ret += binhacks_apply(stage.binhacks.data(), stage.binhacks.size(), hMod);
	if (!(flags & RUNCFG_STAGE_SKIP_BREAKPOINTS)) {
		// FIXME: this workaround is needed, because breakpoints don't check what they overwrite
		if (!(ret != 0 && stage_num == 0 && run_cfg.stages.size() >= 2)) {
			ret += breakpoints_apply(stage.breakpoints.data(), stage.breakpoints.size(), hMod);
		}
	}

	return ret == 0;
}
