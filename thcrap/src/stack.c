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
	log_printf("(JSON) Recherche de %s... ", fn);

	json_array_foreach(patch_array, i, patch_obj) {
		json_size += stack_json_load(&ret, patch_obj, fn);
		json_size += stack_json_load(&ret, patch_obj, fn_build);
	}
	if(!ret) {
		log_printf("introuvable\n");
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

	log_printf("(Data) Recherche %s... ", fn_common_ptr);
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
		log_printf("introuvable\n");
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

		rem_arcs_str[0] = 0;
		json_array_foreach(rem_arcs, i, archive_obj) {
			strcat(rem_arcs_str, "\t");
			strcat(rem_arcs_str, json_string_value(archive_obj));
			strcat(rem_arcs_str, "\n");
		}
		log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
			"Certains patchs n'ont pas été trouvés dans votre configuration:\n"
			"\n"
			"%s"
			"\n"
			"Veuillez reconfigurer votre sélection de patchs - soit en executant l'outil de configuration, "
			"soit en éditant votre fichier de configuration à la main (%s).",
			rem_arcs_str, json_object_get_string(runconfig_get(), "run_cfg_fn")
		);
		VLA_FREE(rem_arcs_str);
	}
	json_decref(rem_arcs);
}
