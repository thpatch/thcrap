/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Patch stack evaluators and information.
  */

#include "thcrap.h"

// Helper function for stack_json_resolve.
size_t stack_json_load(json_t **json_inout, json_t *patch_info, const char *fn)
{
	size_t file_size = 0;
	if(fn && json_inout) {
		json_t *json_new = patch_json_load(patch_info, fn, &file_size);
		if(json_new) {
			patch_print_fn(patch_info, fn);
			if(!*json_inout) {
				*json_inout = json_new;
			} else {
				json_object_merge(*json_inout, json_new);
				json_decref(json_new);
			}
		}
	}
	return file_size;
}

json_t* stack_json_resolve(const char *fn, size_t *file_size)
{
	char *fn_build = NULL;
	json_t *ret = NULL;
	json_t *patch_array = json_object_get(run_cfg, "patches");
	json_t *patch_obj;
	size_t i;
	size_t json_size = 0;

	if(!fn) {
		return NULL;
	}
	fn_build = fn_for_build(fn);
	log_printf("(JSON) Resolving %s... ", fn);

	json_array_foreach(patch_array, i, patch_obj) {
		json_size += stack_json_load(&ret, patch_obj, fn);
		json_size += stack_json_load(&ret, patch_obj, fn_build);
	}
	if(!ret) {
		log_printf("not found\n");
	} else {
		log_printf("\n");
	}
	if(file_size) {
		*file_size = json_size;
	}
	SAFE_FREE(fn_build);
	return ret;
}

void* stack_game_file_resolve(const char *fn, size_t *file_size)
{
	void *ret = NULL;
	int i;
	json_t *patch_array = json_object_get(run_cfg, "patches");

	// Meh, const correctness.
	char *fn_common = fn_for_game(fn);
	const char *fn_common_ptr = fn_common ? fn_common : fn;
	char *fn_build = fn_for_build(fn_common_ptr);

	if(!fn) {
		return NULL;
	}

	log_printf("(Data) Resolving %s... ", fn_common_ptr);
	// Patch stack has to be traversed backwards because later patches take
	// priority over earlier ones, and build-specific files are preferred.
	for(i = json_array_size(patch_array) - 1; i > -1; i--) {
		json_t *patch_obj = json_array_get(patch_array, i);
		const char *log_fn = NULL;
		ret = NULL;

		if(fn_build) {
			ret = patch_file_load(patch_obj, fn_build, file_size);
			log_fn = fn_build;
		}
		if(!ret) {
			ret = patch_file_load(patch_obj, fn_common_ptr, file_size);
			log_fn = fn_common_ptr;
		}
		if(ret) {
			patch_print_fn(patch_obj, log_fn);
			break;
		}
	}
	SAFE_FREE(fn_common);
	SAFE_FREE(fn_build);
	if(!ret) {
		log_printf("not found\n");
	} else {
		log_printf("\n");
	}
	return ret;
}

json_t* stack_game_json_resolve(const char *fn, size_t *file_size)
{
	char *full_fn = fn_for_game(fn);
	json_t *ret = stack_json_resolve(full_fn, file_size);
	SAFE_FREE(full_fn);
	return ret;
}
