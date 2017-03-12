/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Patch stack evaluators and information.
  */

#include "thcrap.h"

json_t* resolve_chain(const char *fn)
{
	json_t *ret = fn ? json_array() : NULL;
	char *fn_build = fn_for_build(fn);
	json_array_append_new(ret, json_string(fn));
	json_array_append_new(ret, json_string(fn_build));
	SAFE_FREE(fn_build);
	return ret;
}

json_t* resolve_chain_game(const char *fn)
{
	char *fn_common = fn_for_game(fn);
	const char *fn_common_ptr = fn_common ? fn_common : fn;
	json_t *ret = resolve_chain(fn_common_ptr);
	SAFE_FREE(fn_common);
	return ret;
}

int stack_chain_iterate(stack_chain_iterate_t *sci, const json_t *chain, sci_dir_t direction)
{
	int ret = 0;
	size_t chain_size = json_array_size(chain);
	if(sci && direction && chain_size) {
		int chain_idx;
		// Setup
		if(!sci->patches) {
			sci->patches = json_object_get(run_cfg, "patches");
			sci->step =
				(direction < 0) ? (json_array_size(sci->patches) * chain_size) - 1 : 0
			;
		} else {
			sci->step += direction;
		}
		chain_idx = sci->step % chain_size;
		sci->fn = json_array_get_string(chain, chain_idx);
		if(chain_idx == (direction < 0) * (chain_size - 1)) {
			sci->patch_info = json_array_get(sci->patches, sci->step / chain_size);
		}
		ret = sci->patch_info != NULL;
	}
	return ret;
}

json_t* stack_json_resolve_chain(const json_t *chain, size_t *file_size)
{
	json_t *ret = NULL;
	stack_chain_iterate_t sci = {0};
	size_t json_size = 0;
	while(stack_chain_iterate(&sci, chain, SCI_FORWARDS)) {
		json_size += patch_json_merge(&ret, sci.patch_info, sci.fn);
	}
	log_printf(ret ? "\n" : "not found\n");
	if(file_size) {
		*file_size = json_size;
	}
	return ret;
}

json_t* stack_json_resolve(const char *fn, size_t *file_size)
{
	json_t *ret = NULL;
	json_t *chain = resolve_chain(fn);
	if(json_array_size(chain)) {
		log_printf("(JSON) Resolving %s... ", fn);
		ret = stack_json_resolve_chain(chain, file_size);
	}
	json_decref(chain);
	return ret;
}

void* stack_file_resolve_chain(const json_t *chain, size_t *file_size)
{
	void *ret = NULL;
	stack_chain_iterate_t sci = {0};

	// Empty stacks are a thing, too.
	if(file_size) {
		*file_size = 0;
	}

	// Both the patch stack and the chain have to be traversed backwards: Later
	// patches take priority over earlier ones, and build-specific files are
	// preferred over generic ones.
	while(stack_chain_iterate(&sci, chain, SCI_BACKWARDS) && !ret) {
		ret = patch_file_load(sci.patch_info, sci.fn, file_size);
		if(ret) {
			patch_print_fn(sci.patch_info, sci.fn);
		}
	}
	log_printf(ret ? "\n" : "not found\n");
	return ret;
}

void* stack_game_file_resolve(const char *fn, size_t *file_size)
{
	void *ret = NULL;
	json_t *chain = resolve_chain_game(fn);
	if(json_array_size(chain)) {
		log_printf("(Data) Resolving %s... ", json_array_get_string(chain, 0));
		ret = stack_file_resolve_chain(chain, file_size);
	}
	json_decref(chain);
	return ret;
}

json_t* stack_game_json_resolve(const char *fn, size_t *file_size)
{
	char *full_fn = fn_for_game(fn);
	json_t *ret = stack_json_resolve(full_fn, file_size);
	SAFE_FREE(full_fn);
	return ret;
}

void stack_show_missing(void)
{
	json_t *patches = json_object_get(run_cfg, "patches");
	size_t i;
	json_t *patch_info;
	json_t *rem_arcs = json_array();
	// Don't forget the null terminator...
	size_t rem_arcs_str_len = 1;

	json_array_foreach(patches, i, patch_info) {
		json_t *archive = json_object_get(patch_info, "archive");
		if(json_is_string(archive) && !PathFileExists(json_string_value(archive))) {
			json_array_append(rem_arcs, archive);
			rem_arcs_str_len += 1 + json_string_length(archive) + 1;
		}
	}

	if(json_array_size(rem_arcs) > 0) {
		VLA(char, rem_arcs_str, rem_arcs_str_len);
		size_t i;
		json_t *archive_obj;
		char *p = rem_arcs_str;

		json_array_foreach(rem_arcs, i, archive_obj) {
			if(json_is_string(archive_obj)) {
				p += sprintf(p, "\t%s\n", json_string_value(archive_obj));
			}
		}
		log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
			"Some patches in your configuration could not be found:\n"
			"\n"
			"%s"
			"\n"
			"Please reconfigure your patch stack - either by running the configuration tool, "
			"or by simply editing your run configuration file (%s).",
			rem_arcs_str, json_object_get_string(runconfig_get(), "run_cfg_fn")
		);
		VLA_FREE(rem_arcs_str);
	}
	json_decref(rem_arcs);
}

int stack_game_covered_by(const char *patch_id)
{
	auto runconfig = runconfig_get();
	auto game = json_object_get(runconfig, "game");

	if(!json_is_string(game)) {
		return 0;
	}

	auto patches = json_object_get(runconfig, "patches");
	size_t i;
	json_t *patch_info;

	json_array_foreach(patches, i, patch_info) {
		const char *id = json_object_get_string(patch_info, "id");
		if(!strcmp(id, patch_id)) {
			size_t game_js_len = json_string_length(game) + strlen(".js") + 1;
			VLA(char, game_js, game_js_len);
			sprintf(game_js, "%s.js", json_string_value(game));

			auto ret = patch_file_exists(patch_info, game_js);
			VLA_FREE(game_js);
			return ret;
		}
	}
	return 0;
}
