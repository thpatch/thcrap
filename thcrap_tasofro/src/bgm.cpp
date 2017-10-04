/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * VFS generators for music names and comments
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"
#include <vfs.h>
#include "bgm.h"

json_t* bgm_generator(std::unordered_map<std::string, json_t*> in_data, const std::string out_fn, size_t* out_size)
{
	if (out_size) {
		*out_size = 0;
	}

	// Find themes.js and musiccmt.js in the input files list
	char* musiccmt_fn = fn_for_game("musiccmt.js");
	auto themes_it = in_data.find("themes.js");
	auto musiccmt_it = in_data.find(musiccmt_fn);
	SAFE_FREE(musiccmt_fn);
	if (themes_it == in_data.end() || musiccmt_it == in_data.end()) {
		return NULL;
	}
	json_t *themes = themes_it->second;
	json_t *musiccmt = musiccmt_it->second;

	json_t *ret = json_object();
	const char *key;
	json_t *value;

	// Extract music names
	const char *game = json_object_get_string(runconfig_get(), "game");
	json_object_foreach(themes, key, value) {
		if (!game || strncmp(key, game, strlen(game)) != 0 || key[strlen(game)] != '_') {
			continue;
		}
		json_t *line = json_object();
		size_t track = atoi(key + strlen(game) + 1);

		json_object_set(line, "7", value);
		*out_size += json_string_length(value);

		json_object_set_new(ret, std::to_string(track).c_str(), line);
	}

	// Extract music comments
	json_object_foreach(musiccmt, key, value) {
		size_t track = atoi(key);
		json_t *line = json_object_get_create(ret, std::to_string(track).c_str(), JSON_OBJECT);

		std::string comment;
		size_t i;
		json_t *comment_line;
		json_flex_array_foreach(value, i, comment_line) {
			if (i != 0) {
				comment += "\n";
			}
			comment += json_string_value(comment_line);
		}
		json_object_set_new(line, "9", json_string(comment.c_str()));
		*out_size += comment.size();
	}

	*out_size += 512; // Because these files are zipped, guessing their exact output size is hard. We'll add a few more bytes.
	return ret;
}
