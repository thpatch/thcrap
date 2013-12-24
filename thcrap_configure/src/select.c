/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#include <thcrap.h>
#include <thcrap_update/src/update.h>
#include "configure.h"
#include "search.h"

#define PATCH_ID_LEN 16

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
			printf(" [%2d] %-*s%s\n", ++list_count, PATCH_ID_LEN, patch_id_str, patch_title);
		}
	}
	if(!print_header) {
		printf("\n");
	}
	return list_count;
}

json_t* SelectPatchStack(json_t *repo_js)
{
	json_t *list_order = json_array();
	json_t *sel_stack = json_array();
	// Screen clearing offset line
	SHORT y;

	json_t *patches = json_object_get(repo_js, "patches");
	json_t *patches_sorted = json_object_get_keys_sorted(patches);
	json_object_set(repo_js, "patches_sorted", patches_sorted);

	if(!json_object_size(patches)) {
		log_printf("\nNo patches available -.-\n");
		goto end;
	}

	cls(0);

	log_printf("-----------------\n");
	log_printf("Selecting patches\n");
	log_printf("-----------------\n");
	log_printf(
		"\n"
		"\n"
	);
	{
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		GetConsoleScreenBufferInfo(hConsole, &csbi);
		y = csbi.dwCursorPosition.Y;
	}

	while(1) {
		size_t list_count = 0;
		char buf[16];
		size_t list_pick;

		json_array_clear(list_order);

		cls(y);

		if(json_array_size(sel_stack)) {
			size_t i;
			json_t *sel;

			printf("Selected patches (in ascending order of priority):\n\n");

			json_array_foreach(sel_stack, i, sel) {
				const char *patch_id = json_array_get_string(sel, 1);
				const char *patch_title = json_object_get_string(patches, patch_id);

				printf("  %2d. %-*s%s\n", ++list_count, PATCH_ID_LEN, patch_id, patch_title);

				json_array_append(list_order, sel);
			}
			printf("\n\n");
		}

		list_count = RepoPrintPatches(list_order, repo_js, sel_stack);
		printf("\n");

		printf(
			"Select a patch to add to the stack"
			"\n (1 - %u, select a number again to remove, anything else to cancel): ",
		list_count);

		fgets(buf, sizeof(buf), stdin);

		if(
			(sscanf(buf, "%u", &list_pick) != 1) ||
			(list_pick > list_count)
		) {
			break;
		}

		if(list_pick > json_array_size(sel_stack)) {
			json_t *sel = json_array_get(list_order, list_pick - 1);
			json_array_append(sel_stack, sel);
		} else {
			json_array_remove(sel_stack, list_pick - 1);
		}
	}
end:
	json_decref(list_order);
	return sel_stack;
}
