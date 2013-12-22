/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#include <thcrap.h>
#include <thcrap_update/src/update.h>
#include "configure.h"
#include "search.h"

#define PATCH_ID_LEN 16

// Returns 1 if [patch_id] is [selected].
int PatchIsSelected(json_t *selected, json_t *patch_id)
{
	json_t *sel_val;
	size_t i;

	json_array_foreach(selected, i, sel_val) {
		if(json_equal(patch_id, sel_val)) {
			return 1;
		}
	}
	return 0;
}

json_t* SelectPatchStack(json_t *server_js, json_t *selected)
{
	json_t *list_order = json_array();
	// Screen clearing offset line
	SHORT y;

	json_t *patches = json_object_get(server_js, "patches");
	json_t *patches_sorted = json_object_get_keys_sorted(patches);

	if(!json_object_size(patches)) {
		log_printf("\nNo patches available -.-\n");
		return 0;
	}
	if(!json_is_array(selected)) {
		selected = json_array();
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

		const char *url_desc = json_object_get_string(server_js, "url_desc");
		if(url_desc) {
			log_printf(
				"For more information on these patches, visit\n"
				"\n"
				"\t%s"
				"\n"
				"\n"
				"\n",
				url_desc
			);
		}

		GetConsoleScreenBufferInfo(hConsole, &csbi);
		y = csbi.dwCursorPosition.Y;
	}

	while(1) {
		size_t list_count = 0;
		char buf[16];
		size_t list_pick;
		json_t *patch;

		json_array_clear(list_order);

		cls(y);

		if(json_array_size(selected)) {
			size_t i;
			json_t *json_val;

			printf("Selected patches (in ascending order of priority):\n\n");

			json_array_foreach(selected, i, json_val) {
				const char *patch_id = json_string_value(json_val);
				const char *patch_title = json_object_get_string(patches, patch_id);

				printf("  %2d. %-*s%s\n", ++list_count, PATCH_ID_LEN, patch_id, patch_title);

				json_array_append(list_order, json_val);
			}
			printf("\n\n");
		}

		if(json_array_size(selected) < json_object_size(patches)) {
			json_t *json_val;
			size_t i;
			printf("Available patches:\n\n");

			json_array_foreach(patches_sorted, i, json_val) {
				const char *patch_id = json_string_value(json_val);
				const char *patch_title = json_object_get_string(patches, patch_id);

				if(!PatchIsSelected(selected, json_val)) {
					printf(" [%2d] %-*s%s\n", ++list_count, PATCH_ID_LEN, patch_id, patch_title);

					json_array_append(list_order, json_val);
				}
			}
			printf("\n\n");
		}

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
		patch = json_array_get(list_order, list_pick - 1);

		if(!strcmp(json_string_value(patch), "base_tsa")) {
			printf(
				"\nUm... you _really_ do not want to mess with base_tsa.\n"
				"This patch supplies the foundation for every other patch offered here.\n"
				"If you remove it, none of those will work.\n\n");
			pause();
		} else {
			if(list_pick > json_array_size(selected)) {
				json_array_append(selected, patch);
			} else {
				json_array_remove(selected, list_pick - 1);
			}
		}
	}
	json_decref(list_order);
	json_decref(patches_sorted);
	return selected;
}
