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
	HackpointMemoryPage binhack_page = { NULL, 0 };
	HackpointMemoryPage codecave_pages[5] = { { NULL, 0 }, { NULL, 0 }, { NULL, 0 }, { NULL, 0 }, { NULL, 0 } };
	HackpointMemoryPage breakpoint_pages[2] = { { NULL, 0 }, { NULL, 0 } };
};

struct runconfig_t
{
	// Run configuration in json format (needed for extensions by plugins)
	json_t *json = nullptr;
	// Thcrap directory (from runtime)
	std::string thcrap_dir;
	// Run configuration path (from runtime)
	std::vector<std::string> runcfg_fn;
	// Game ID, for example "th06" (from game_id.js)
	std::string_view game;
	// Game build, for example "v1.00a" (from game_id.js)
	std::string build;
	// Command line parameters passed to the exe
	std::string_view cmdline;
	// Original game title, for example "東方紅魔郷　～ the Embodiment of Scarlet Devil" (from game_id.js)
	std::string_view title;
	// URL to download the last game update (from game_id.js)
	std::string_view update_url;
	// Array of latest builds. For example, [ "v0.13", "v1.02h" ] (from game_id.js)
	std::vector<std::string_view> latest;
	// dat dump path if dat dump is enabled, empty otherwise (from runcfg)
	std::string_view dat_dump;
	// Hackpoints (breakpoints, codecaves and binhacks) for stages (from game_id.js and game_id.build.js).
	// The global hackpoints (those not in a stage) are put in the last stage.
	std::vector<stage_t> stages;
	// Boolean flag that tells the binhack parser if it should show a message box, should it fail to find a function
	bool msgbox_invalid_func;
};

static runconfig_t run_cfg;

#pragma optimize("y", off)
HackpointMemoryName locate_address_in_stage_pages(void* FindAddress) {
	uint8_t* address = (uint8_t*)FindAddress;
	const size_t stage_count = run_cfg.stages.size();

#define PageIsValidAndContainsAddress(page, addr) \
	((page).address && ((addr) >= (page).address) & ((addr) <= (page).address + (page).size))

	for (size_t i = 0; i < stage_count; ++i) {
		stage_t& stage = run_cfg.stages[i];
		if (PageIsValidAndContainsAddress(stage.binhack_page, address)) {
			return { "Binhack Sourcecave", (size_t)(address - stage.binhack_page.address), i };
		}
		for (size_t j = 0; j < elementsof(stage.codecave_pages); ++j) {
			if (PageIsValidAndContainsAddress(stage.codecave_pages[j], address)) {
				codecave_t* found_cave = NULL;
				uint8_t* found_cave_addr = NULL;
				for (codecave_t& codecave : stage.codecaves) {
					if (codecave.virtual_address &&
						(codecave.virtual_address <= address) &&
						(codecave.virtual_address > found_cave_addr)
					) {
						found_cave_addr = codecave.virtual_address;
						found_cave = &codecave;
					}
				}
				if (found_cave) {
					return { found_cave->name, (size_t)(address - found_cave->virtual_address), i };
				}
			}
		}
		if (PageIsValidAndContainsAddress(stage.breakpoint_pages[0], address)) {
			return { "Breakpoint Sourcecave", (size_t)(address - stage.breakpoint_pages[0].address), i };
		}
		if (PageIsValidAndContainsAddress(stage.breakpoint_pages[1], address)) {
			return { "Breakpoint Callcave", (size_t)(address - stage.breakpoint_pages[1].address), i };
		}
	}
	return { NULL, 0, 0 };
#undef PageIsValidAndContainsAddress
}
#pragma optimize("", on)

static void runconfig_stage_load(json_t *stage_json)
{
	stage_t& stage = run_cfg.stages.emplace_back();
	const char *key;
	json_t *value;

	stage.module = (HMODULE)json_object_get_eval_int_default(stage_json, "module", NULL, JEVAL_STRICT | JEVAL_NO_EXPRS);

	json_t *options = json_object_get(stage_json, "options");
	if (options) {
		patch_opts_from_json(options);
	}

	json_t *codecaves = json_object_get(stage_json, "codecaves");
	json_object_foreach_fast(codecaves, key, value) {
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

	json_t *binhacks = json_object_get(stage_json, "binhacks");
	json_object_foreach_fast(binhacks, key, value) {
		binhack_t binhack;
		if (!binhack_from_json(key, value, &binhack)) {
			continue;
		}
		stage.binhacks.push_back(binhack);
	}

	json_t *breakpoints = json_object_get(stage_json, "breakpoints");
	json_object_foreach_fast(breakpoints, key, value) {

		breakpoint_t breakpoint;
		if (!breakpoint_from_json(key, value, &breakpoint)) {
			continue;
		}
		stage.breakpoints.push_back(breakpoint);
	}
}

int BP_runtime_apply_stage_by_name(x86_reg_t *regs, json_t *bp_info) {

	// Use stage_name to lookup which hackpoints to apply
	if (const char* stage_name = (char*)json_object_get_immediate(bp_info, regs, "stage_name")) {
		json_t* stage_list = json_object_get(bp_info, "stages_list");
		if (json_t* stage_data = json_object_get(stage_list, stage_name)) {
			size_t next_stage_index = run_cfg.stages.size();
			runconfig_stage_load(stage_data);
			HMODULE module_base_addr = (HMODULE)json_object_get_immediate(stage_data, regs, "base_addr");
			runconfig_stage_apply(next_stage_index, RUNCFG_STAGE_USE_MODULE, module_base_addr);
		}
	}
	return 1;
}

int BP_runtime_apply_stage_by_address(x86_reg_t *regs, json_t *bp_info) {
	if (void* address_from_module = (void*)json_object_get_immediate(bp_info, regs, "address_expr")) {
		HMODULE module_base_addr;
		if (GetModuleHandleExA(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, /* dwFlags */
			(char*)address_from_module, /* lpModuleName (NOT A STRING WITH THIS FLAG)*/
			&module_base_addr /* phModule */
		)) {
			size_t mod_name_len = GetModuleFileNameU(module_base_addr, NULL, 0) + 1;
			VLA(char, mod_name, mod_name_len);
			GetModuleFileNameU(module_base_addr, mod_name, mod_name_len);
			const char* stage_name = PathFindFileNameU(mod_name);
			json_t* stage_list = json_object_get(bp_info, "stages_list");
			if (json_t* stage_data = json_object_get(stage_list, stage_name)) {
				size_t next_stage_index = run_cfg.stages.size();
				runconfig_stage_load(stage_data);
				runconfig_stage_apply(next_stage_index, RUNCFG_STAGE_USE_MODULE, module_base_addr);
			}
			VLA_FREE(mod_name);
		}
	}
	return 1;
}

// These variants don't seem useful immediately,
// but they're here just in case.
/*
int BP_runtime_apply_stage_by_number(x86_reg_t *regs, json_t *bp_info) {
	if (size_t lookup_value = json_object_get_immediate(bp_info, regs, "lookup_value")) {
		json_t* stage_list = json_object_get(bp_info, "stages_list");
		if (json_t* stage_data = json_object_numkey_get(stage_list, (json_int_t)lookup_value)) {
			size_t next_stage_index = run_cfg.stages.size();
			runconfig_stage_load(stage_data);
			HMODULE module_base_addr = (HMODULE)json_object_get_immediate(stage_data, regs, "base_addr");
			runconfig_stage_apply(next_stage_index, RUNCFG_STAGE_USE_MODULE, module_base_addr);
		}
	}
	return 1;
}

int BP_runtime_apply_stage_by_expr(x86_reg_t *regs, json_t *bp_info) {
	if (size_t lookup_value = json_object_get_immediate(bp_info, regs, "lookup_expr")) {
		json_t* stage_list = json_object_get(bp_info, "stages_list");
		json_t* stage_data;
		const char* list_expr;
		json_object_foreach_fast(stage_list, list_expr, stage_data) {
			size_t stage_result;
			if (eval_expr(list_expr, '\0', &stage_result, regs, NULL, NULL)) {
				if (lookup_value != stage_result) {
					continue;
				}
				size_t next_stage_index = run_cfg.stages.size();
				runconfig_stage_load(stage_data);
				HMODULE module_base_addr = (HMODULE)json_object_get_immediate(stage_data, regs, "base_addr");
				runconfig_stage_apply(next_stage_index, RUNCFG_STAGE_USE_MODULE, module_base_addr);
				break;
			}
		}
	}
	return 1;
}
*/

void runconfig_load(json_t *file, int flags)
{
	bool can_overwrite = (~flags & RUNCONFIG_NO_OVERWRITE);
	bool load_binhacks = (~flags & RUNCONFIG_NO_BINHACKS);

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

	auto set_string_if_exist = [=](const char* key, auto& out) {
		if (can_overwrite || out.empty()) {
			if (json_t* value = json_object_get(file, key)) {
				out = json_string_value(value);
			}
		}
	};

	set_string_if_exist("game",  run_cfg.game);
	set_string_if_exist("build", run_cfg.build);
	set_string_if_exist("cmdline", run_cfg.cmdline);
	set_string_if_exist("title", run_cfg.title);
	set_string_if_exist("url_update", run_cfg.update_url);

	run_cfg.msgbox_invalid_func = json_object_get_eval_bool_default(file, "msgbox_invalid_func", false, JEVAL_NO_EXPRS);

	if (can_overwrite || run_cfg.dat_dump.empty()) {
		if (json_t* value = json_object_get(file, "dat_dump")) {
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
	}

	json_t* value;
	json_t* filenames = json_object_get(file, "runcfg_fn");
	json_flex_array_foreach_scoped(size_t, i, filenames, value) {
		if (json_is_string(value)) {
			run_cfg.runcfg_fn.push_back(json_string_value(value));
		}
	}

	if (can_overwrite || run_cfg.latest.empty()) {
		if (json_t* value = json_object_get(file, "latest")) {
			json_t *latest_entry;

			run_cfg.latest.clear();
			json_flex_array_foreach_scoped(size_t, i, value, latest_entry) {
				if (json_is_string(latest_entry)) {
					run_cfg.latest.push_back(json_string_value(latest_entry));
				}
			}
		}
	}

	if (json_t* value = json_object_get(file, "patches")) {
		json_t *patch;
		json_array_foreach_scoped(size_t, i, value, patch) {
			stack_add_patch_from_json(patch);
		}
	}

	if (json_t* value = json_object_get(file, "detours")) {
		const char* dll_name;
		json_t* detours;
		json_object_foreach_fast(value, dll_name, detours) {
			if (!detours) continue;
			switch (json_typeof(detours)) {
				case JSON_NULL:
					detour_disable(dll_name, NULL);
					break;
				case JSON_OBJECT: {
					const char* func_name;
					json_t* func_json;
					json_object_foreach_fast(detours, func_name, func_json) {
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
			json_flex_array_foreach_scoped(size_t, i, stages, value) {
				runconfig_stage_load(value);
			};
			runconfig_stage_load(file);
		}
	}
}

void runconfig_load_from_file(const char *path)
{
	json_t *new_cfg = json_load_file_report(path);
	runconfig_load(new_cfg, RUNCFG_STAGE_DEFAULT);
	run_cfg.runcfg_fn.push_back(path);
	json_decref(new_cfg);
}

void runconfig_print()
{
	log_print(
		"---------------------------\n"
		"Complete run configuration:\n"
		"---------------------------\n"
	);
	if (run_cfg.runcfg_fn.size() == 1) {
		log_printf("  runcfg fn: '%s'\n", run_cfg.runcfg_fn[0].c_str());
	}
	else {
		log_print("  runcfg fn:\n");
		for (auto& it : run_cfg.runcfg_fn) {
			log_printf("    '%s'\n", it.c_str());
		}
	}
	log_printf("  thcrap dir: '%s'\n",   run_cfg.thcrap_dir.c_str());
	log_printf("  game id: '%s'\n",      run_cfg.game.data());
	log_printf("  build id: '%s'\n",     run_cfg.build.c_str());
	log_printf("  game title: '%s'\n",   run_cfg.title.data());
	log_printf("  command line: '%s'\n", run_cfg.cmdline.data());
	log_printf("  update URL: '%s'\n",   run_cfg.update_url.data());
	log_print ("  latest:");
	for (const auto& it : run_cfg.latest) {
		log_printf(" '%s'", it.data());
	}
	log_print("\n");
	log_printf("  dat_dump: '%s'\n", run_cfg.dat_dump.data());
	log_print(
		"---------------------------\n"
		"Patch stack:\n"
		"---------------------------\n"
	);
	stack_print();
	log_print(
		"---------------------------\n"
		"Run configuration JSON:\n"
		"---------------------------\n"
	);
	json_dump_log(run_cfg.json, JSON_INDENT(2));
	log_print(
		"\n---------------------------\n"
	);
}

void runconfig_free()
{
	stack_free();
	run_cfg.json = json_decref_safe(run_cfg.json);
	assert(!run_cfg.json);
	run_cfg.thcrap_dir.clear();
	run_cfg.runcfg_fn.clear();
	run_cfg.game = {};
	run_cfg.build.clear();
	run_cfg.cmdline = {};
	run_cfg.title = {};
	run_cfg.update_url = {};
	run_cfg.latest.clear();
	run_cfg.dat_dump = {};
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
			free((void*)breakpoint.expected);
			for (size_t i = 0; breakpoint.addr[i].type != END_ADDR; ++i) {
				if (breakpoint.addr[i].str) {
					free((void*)breakpoint.addr[i].str);
				}
			}
			free(breakpoint.addr);
			json_decref(breakpoint.json_obj);
		}
		stage.breakpoints.clear();
		if (stage.binhack_page.address) {
			VirtualFree(stage.binhack_page.address, 0, MEM_RELEASE);
		}
		for (size_t i = 0; i < elementsof(stage.codecave_pages); ++i) {
			if (stage.codecave_pages[i].address) {
				VirtualFree(stage.codecave_pages[i].address, 0, MEM_RELEASE);
			}
		}
		for (size_t i = 0; i < elementsof(stage.breakpoint_pages); ++i) {
			if (stage.breakpoint_pages[i].address) {
				VirtualFree(stage.breakpoint_pages[i].address, 0, MEM_RELEASE);
			}
		}
	}
	run_cfg.stages.clear();
}

const json_t *runconfig_json_get()
{
	return run_cfg.json;
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
	return run_cfg.runcfg_fn.empty() == false ? run_cfg.runcfg_fn[0].c_str() : nullptr;
}

const char *runconfig_game_get()
{
	return run_cfg.game.data();
}

const char *runconfig_build_get()
{
	return run_cfg.build.empty() == false ? run_cfg.build.c_str() : nullptr;
}

const char *runconfig_cmdline_get()
{
	return run_cfg.cmdline.data();
}

std::string_view runconfig_game_get_view()
{
	return run_cfg.game;
}

std::string_view runconfig_build_get_view()
{
	return run_cfg.build;
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
		const json_t *title = strings_get(run_cfg.game.data());
		if (title) {
			return json_string_value(title);
		}
	}

	// Get the title from [game_id].js
	if (!run_cfg.title.empty()) {
		return run_cfg.title.data();
	}

	// Fallback to the game id
	if (!run_cfg.game.empty()) {
		return run_cfg.game.data();
	}

	// Nothing worked
	return nullptr;
}

const char *runconfig_update_url_get()
{
	return run_cfg.update_url.data();
}

const char *runconfig_dat_dump_get()
{
	return run_cfg.dat_dump.data();
}

bool runconfig_latest_check()
{
	if (run_cfg.latest.empty()) {
		return true;
	}

	for (const auto& it : run_cfg.latest) {
		if (run_cfg.build == it) {
			return true;
		}
	}

	return false;
}

const char *runconfig_latest_get()
{
	if (!run_cfg.latest.empty()) {
		return run_cfg.latest.back().data();
	}
	return nullptr;
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

	if (!(flags & RUNCFG_STAGE_USE_MODULE)) {
		module = stage.module;
	}

	failed += codecaves_apply(stage.codecaves.data(), stage.codecaves.size(), module, stage.codecave_pages);
	failed += binhacks_apply(stage.binhacks.data(), stage.binhacks.size(), module, &stage.binhack_page);
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
		failed += breakpoints_apply(stage.breakpoints.data(), breakpoint_count, module, stage.breakpoint_pages);
	}

	return failed == 0;
}
