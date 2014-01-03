/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#include <thcrap.h>
#include <thcrap_update/src/update.h>
#include "configure.h"
#include "search.h"

json_t *dep_to_sel(const char *dep_str)
{
	if(dep_str) {
		const char *slash = strrchr(dep_str, '/');
		if(slash) {
			return json_pack("[s#s]", dep_str, slash - dep_str, slash + 1);
		} else {
			return json_pack("[ns]", dep_str);
		}
	}
	return NULL;
}

// Returns 1 if the patch [patch_id] from [repo_id] is in [sel_stack].
// [repo_id] can be NULL to ignore the repository.
int IsSelected(json_t *sel_stack, json_t *repo_id, json_t *patch_id)
{
	json_t *sel;
	size_t i;

	json_array_foreach(sel_stack, i, sel) {
		json_t *sel_repo = json_array_get(sel, 0);
		json_t *sel_patch = json_array_get(sel, 1);
		if(
			(json_is_string(repo_id) ? json_equal(sel_repo, repo_id) : 1)
			&& json_equal(sel_patch, patch_id)
		) {
			return 1;
		}
	}
	return 0;
}

// Locates a repository for [sel] in [repo_list], starting from [orig_repo_id].
const char* SearchPatch(json_t *repo_list, const char *orig_repo_id, const json_t *sel)
{
	const char *repo_id = json_array_get_string(sel, 0);
	const char *patch_id = json_array_get_string(sel, 1);
	const json_t *orig_repo = json_object_get(repo_list, orig_repo_id);
	const json_t *patches = json_object_get(orig_repo, "patches");
	const json_t *remote_repo;
	const char *key;

	// Absolute dependency
	// (in fact, just a check to see wheter the patch is actually available)
	if(repo_id) {
		remote_repo = json_object_get(repo_list, repo_id);
		patches = json_object_get(remote_repo, "patches");
		return json_object_get(patches, patch_id) ? repo_id : NULL;
	}

	// Relative dependency
	if(json_object_get(patches, patch_id)) {
		return orig_repo_id;
	}

	json_object_foreach(repo_list, key, remote_repo) {
		patches = json_object_get(remote_repo, "patches");
		if(json_object_get(patches, patch_id)) {
			return key;
		}
	}
	// Not found...
	return NULL;
}

// Adds a patch and, recursively, all of its required dependencies. These are
// resolved first on the repository the patch originated, then globally.
// Returns the number of missing dependencies.
int AddPatch(json_t *sel_stack, json_t *repo_list, json_t *sel)
{
	int ret = 0;
	json_t *patches = json_object_get(runconfig_get(), "patches");
	const char *repo_id = json_array_get_string(sel, 0);
	const char *patch_id = json_array_get_string(sel, 1);
	const json_t *repo = json_object_get(repo_list, repo_id);
	json_t *repo_servers = json_object_get(repo, "servers");
	json_t *patch_info = patch_bootstrap(sel, repo_servers);
	json_t *patch_full = patch_init(patch_info);
	json_t *dependencies = json_object_get(patch_full, "dependencies");
	size_t i;
	json_t *dep;

	json_array_foreach(dependencies, i, dep) {
		const char *dep_str = json_string_value(dep);
		json_t *dep_sel = dep_to_sel(dep_str);
		json_t *dep_repo = json_array_get(dep_sel, 0);
		json_t *dep_patch = json_array_get(dep_sel, 1);

		if(!IsSelected(sel_stack, dep_repo, dep_patch)) {
			const char *target_repo = SearchPatch(repo_list, repo_id, dep_sel);
			if(!target_repo) {
				log_printf("ERROR: Dependency '%s' of patch '%s' not met!\n", dep_str, patch_id);
				ret++;
			} else {
				json_array_set_new(dep_sel, 0, json_string(target_repo));
				ret += AddPatch(sel_stack, repo_list, dep_sel);
			}
		}
		json_decref(dep_sel);
	}
	json_array_append(patches, patch_full);
	json_array_append(sel_stack, sel);
	json_decref(patch_full);
	json_decref(patch_info);
	return ret;
}

// Returns the number of patches removed.
int RemovePatch(json_t *sel_stack, size_t id)
{
	int ret = 0;
	json_t *patches = json_object_get(runconfig_get(), "patches");
	json_t *sel = json_array_get(sel_stack, id);
	json_t *patch_id = json_array_get(sel, 1);
	size_t i;
	json_t *patch_info;

	json_array_remove(patches, id);
	json_array_remove(sel_stack, id);

	json_array_foreach(patches, i, patch_info) {
		json_t *dependencies = json_object_get(patch_info, "dependencies");
		size_t j;
		json_t *dep;

		json_array_foreach(dependencies, j, dep) {
			if(json_equal(patch_id, dep)) {
				ret += RemovePatch(sel_stack, id);
				i--;
				break;
			}
		}
	}
	return 1;
}

int PrettyPrintPatch(const char *patch, const char *title)
{
#define LEFT_LEN 20

	char left[LEFT_LEN + 1];
	size_t patch_len = strlen(patch);

	if(!patch || !title) {
		return -1;
	}
	memset(left, ' ', LEFT_LEN);
	if(patch_len < LEFT_LEN) {
		memcpy(left, patch, patch_len);
	} else {
		memcpy(left, patch, LEFT_LEN);
		strcpy(left + LEFT_LEN - strlen("... "), "... ");
	}
	left[LEFT_LEN] = 0;

	printf("%s%s\n", left, title);
	return 0;
}

// Prints all patches of [repo_js] that are not part of the [sel_stack],
// filling [list_order] with the order they appear in.
// Returns the final array size of [list_order].
int RepoPrintPatches(json_t *list_order, json_t *repo_js, json_t *sel_stack)
{
	json_t *patch_id;
	size_t i;
	json_t *patches = json_object_get(repo_js, "patches");
	json_t *patches_sorted = json_object_get(repo_js, "patches_sorted");
	json_t *repo_id = json_object_get(repo_js, "id");
	const char *repo_id_str = json_string_value(repo_id);
	const char *repo_title = json_object_get_string(repo_js, "title");
	size_t list_count = json_array_size(list_order);
	int print_header = 1;

	json_array_foreach(patches_sorted, i, patch_id) {
		const char *patch_id_str = json_string_value(patch_id);
		const char *patch_title = json_object_get_string(patches, patch_id_str);

		if(!IsSelected(sel_stack, repo_id, patch_id)) {
			json_t *sel = json_pack("[ss]", repo_id_str, patch_id_str);
			json_array_append_new(list_order, sel);

			if(print_header) {
				printf("Patches from [%s] (%s):\n\n", repo_title, repo_id_str);
				print_header = 0;
			}
			printf(" [%2d] ", ++list_count);
			PrettyPrintPatch(patch_id_str, patch_title);
		}
	}
	if(!print_header) {
		printf("\n");
	}
	return list_count;
}

int PrintSelStack(json_t *list_order, json_t *repo_list, json_t *sel_stack)
{
	size_t list_count = json_array_size(list_order);
	size_t i;
	json_t *sel;

	if(!json_array_size(sel_stack)) {
		return list_count;
	}
	printf("Patchs selectionnes (Par ordre croissant de priorite):\n\n");

	json_array_foreach(sel_stack, i, sel) {
		const char *repo_id = json_array_get_string(sel, 0);
		const char *patch_id = json_array_get_string(sel, 1);
		const json_t *repo = json_object_get(repo_list, repo_id);
		const json_t *patches = json_object_get(repo, "patches");
		const char *patch_title = json_object_get_string(patches, patch_id);
		json_t *full_id = json_pack("s++", repo_id, "/", patch_id);

		printf("  %2d. ", ++list_count);
		PrettyPrintPatch(json_string_value(full_id), patch_title);

		json_array_append(list_order, sel);
		json_decref(full_id);
	}
	printf("\n");
	return list_count;
}

json_t* SelectPatchStack(json_t *repo_list)
{
	json_t *list_order = json_array();
	json_t *internal_cfg = json_pack("{s[]}", "patches");
	json_t *sel_stack = json_array();
	const char *key;
	json_t *json_val;
	// Total number of required lines in the console buffer
	SHORT buffer_lines = 0;
	size_t list_count = 0;

	if(!json_object_size(repo_list)) {
		log_printf("\nNo repositories available -.-\n");
		goto end;
	}
	// Sort patches
	json_object_foreach(repo_list, key, json_val) {
		json_t *patches = json_object_get(json_val, "patches");
		json_t *patches_sorted = json_object_get_keys_sorted(patches);
		json_object_set_new(json_val, "patches_sorted", patches_sorted);
		buffer_lines += json_array_size(patches_sorted) + 3;
	}
	if(!buffer_lines) {
		log_printf("\nAucun patch disponible -.-\n");
		goto end;
	}

	runconfig_set(internal_cfg);
	if(wine_flag) {
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		// Header (5) + sel_stack (3) + prompt (2)
		COORD buffer = {80, buffer_lines + 5 + 3 + 2};
		SetConsoleScreenBufferSize(hConsole, buffer);
	}
	while(1) {
		char buf[16];
		size_t list_pick;
		size_t stack_size = json_array_size(sel_stack);
		size_t stack_offset;

		list_count = 0;
		json_array_clear(list_order);

		cls(0);

		log_printf("--------------------\n");
		log_printf("Selection des patchs\n");
		log_printf("--------------------\n");
		log_printf(
			"\n"
			"\n"
		);

		json_object_foreach(repo_list, key, json_val) {
			list_count = RepoPrintPatches(list_order, json_val, sel_stack);
		}
		list_count = PrintSelStack(list_order, repo_list, sel_stack);
		printf("\n");

		stack_offset = json_array_size(list_order) - stack_size;

		if(stack_size) {
			printf(
				"(1 - %u to add more, %u - %u to remove from the stack, ENTER to confirm): ",
			stack_offset, stack_offset + 1, list_count);
		} else {
			printf("Pick a patch (1 - %u): ", list_count);
		}
		console_read(buf, sizeof(buf));

		if(
			(sscanf(buf, "%u", &list_pick) != 1) ||
			(list_pick > list_count)
		) {
			if(!stack_size) {
				printf("\nPlease select at least one patch before continuing.\n");
				pause();
				continue;
			}
			break;
		}
		list_pick--;
		if(list_pick < stack_offset) {
			int ret;
			json_t *sel = json_array_get(list_order, list_pick);
			const char *repo_id = json_array_get_string(sel, 0);
			const char *patch_id = json_array_get_string(sel, 1);
			log_printf("Resolving dependencies for %s/%s...\n", repo_id, patch_id);
			ret = AddPatch(sel_stack, repo_list, sel);
			if(ret) {
				log_printf(
					"\n"
					"%d unmet dependencies. This configuration will most likely not work correctly!\n",
					ret
				);
				pause();
			}
		} else {
			RemovePatch(sel_stack, list_pick - stack_offset);
		}
	}
end:
	json_decref(list_order);
	json_decref(internal_cfg);
	return sel_stack;
}
