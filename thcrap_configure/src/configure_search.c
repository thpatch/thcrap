/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#include <thcrap.h>
#include "configure.h"

static const char games_js_fn[] = "games.js";

static const char* ChooseLocation(const char *id, json_t *locs)
{
	size_t num_versions = json_object_size(locs);
	if(num_versions == 1) {
		const char *loc;
		const char *variety;

		loc = json_object_iter_key(json_object_iter(locs));
		variety = json_object_get_string(locs, loc);
		log_printf("Found %s (%s) at %s\n", id, variety, loc);

		return loc;
	} else if(num_versions > 1) {
		const char *loc;
		json_t *val;
		size_t i = 0;
		size_t loc_num;
				
		log_printf("Found %d versions of %s:\n\n", num_versions, id);
				
		json_object_foreach(locs, loc, val) {
			log_printf(" [%2d] %s: %s\n", ++i, loc, json_string_value(val));
		}
		printf("\n");
		while(1) {
			char buf[16];
			printf("Pick a version to run the patch on: (1 - %u): ", num_versions);

			fgets(buf, sizeof(buf), stdin);
			if(
				(sscanf(buf, "%u", &loc_num) == 1) &&
				(loc_num <= num_versions) &&
				(loc_num >= 1)
			) {
				break;
			}
		}
		i = 0;
		json_object_foreach(locs, loc, val) {
			if(++i == loc_num) {
				return loc;
			}
		}
	}
	return NULL;
}

json_t* ConfigureLocateGames(const char *games_js_path)
{
	json_t *games;
	json_t *found;

	cls(0);

	log_printf("--------------\n");
	log_printf("Locating games\n");
	log_printf("--------------\n");
	log_printf(
		"\n"
		"\n"
	);

	games = json_load_file_report("games.js");
	if(json_object_size(games) != 0) {
		log_printf("You already have a %s with the following contents:\n\n", games_js_fn);
		json_dump_log(games, JSON_INDENT(2) | JSON_SORT_KEYS);
		log_printf("\n\n");
		if(Ask("Should the paths of these games be re-scanned?")) {
			json_object_clear(games);
		} else if(!Ask("Should new games be added to this list?")) {
			return games;
		}
	} else {
		games = json_object();
		log_printf(
			"You don't have a %s yet.\n"
			"\n"
			"Thus, I now need to search for all Touhou games installed on your system.\n"
			"This only has to be done once - unless, of course, you later move the games to\n"
			"different directories.\n"
			"\n"
			"Depending on the number of drives and your directory structure,\n"
			"this may take a while. You can speed up this process by giving me the\n"
			"_common_ root path all of your Touhou games share.\n"
			"\n"
			"For example, if you have 'Double Dealing Character' in \n"
			"\n"
				"\tC:\\Games\\Touhou\\DDC\\\n"
			"\n"
			"and 'Imperishable Night' in\n"
			"\n"
				"\tC:\\Games\\Touhou\\IN\\\n"
			"\n"
			"you would now specify \n"
			"\n"
				"\tC:\\Games\\Touhou\\\n"
			"\n"
			"... unless, of course, your Touhou games are spread out all over your drives -\n"
			"in which case there's no way around a complete search.\n"
			"\n"
			"\n",
			games_js_fn
		);
	}

	{
		char search_path[MAX_PATH * 2] = {0};
		while(1)
		{
			size_t search_path_len;
			log_printf(
				"Root path for search\n"
				" (keep empty to search entire system): "
			);
			fgets(search_path, sizeof(search_path), stdin);
			log_printf("\n");

			// Remove that damn \n
			search_path_len = strlen(search_path) + 1 + 1;
			search_path[search_path_len - 3] = 0;

			if(search_path[0] == '\0') {
				break;
			}
			{
				// Ensure UTF-8
				VLA(wchar_t, search_path_w, search_path_len);
				WCHAR_T_CONV(search_path);
				// We can't use StringToUTF8 for constant memory due to UTF8_MUL, so...
				WideCharToMultiByte(CP_UTF8, 0, search_path_w, -1, search_path, sizeof(search_path), NULL, NULL);
			}

			str_slash_normalize_win(search_path);
			PathAddBackslashA(search_path);

			if(!PathFileExists(search_path)) {
				log_printf("Hmm, that path (%s) does not exist.\n", search_path);
			} else {
				break;
			}
		}
		log_printf("Searching... this may take a while...\n\n");
		found = SearchForGames(search_path, games);
	}
	{
		const char *id;
		json_t *locs;
		json_object_foreach(found, id, locs) {
			const char *loc = ChooseLocation(id, locs);
			json_object_set_new(games, id, json_string(loc));
			printf("\n");
		}
	}
	if(json_object_size(found))
	{
		char *games_js_str = NULL;
		FILE* games_js_file;

		SetCurrentDirectory(games_js_path);

		games_js_str = json_dumps(games, JSON_INDENT(2) | JSON_SORT_KEYS);

		games_js_file = fopen(games_js_fn, "w");
		fputs(games_js_str, games_js_file);
		fclose(games_js_file);
		
		log_printf("The following game locations have been identified and written to %s:\n", games_js_fn);
		log_printf(games_js_str);
		log_printf("\n");
		SAFE_FREE(games_js_str);
	} else {
		log_printf("No new game locations found.\n");
	}
	json_decref(found);
	return games;
}
