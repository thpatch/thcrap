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
#include <algorithm>

json_t* bgm_generator(std::unordered_map<std::string, json_t*> in_data, const std::string out_fn, size_t* out_size)
{
	if (out_size) {
		*out_size = 0;
	}

	const char *spell_col = nullptr;
	const char *comment_col = nullptr;
	if (game_id == TH105) {
		spell_col = "1";
	}
	else {
		spell_col = "7";
		comment_col = "9";
	}

	// Find themes.js and musiccmt.js in the input files list
	char* musiccmt_fn = fn_for_game("musiccmt.js");
	auto themes_it = in_data.find("themes.js");
	auto musiccmt_it = in_data.find(musiccmt_fn);
	SAFE_FREE(musiccmt_fn);
	if (themes_it == in_data.end() && musiccmt_it == in_data.end()) {
		return NULL;
	}
	json_t *themes = themes_it->second;
	json_t *musiccmt = nullptr;
	if (musiccmt_it != in_data.end()) {
		musiccmt = musiccmt_it->second;
	}

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

		json_object_set(line, spell_col, value);
		*out_size += json_string_length(value);

		json_object_set_new(ret, std::to_string(track).c_str(), line);
	}

	if (comment_col && musiccmt) {
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
			json_object_set_new(line, comment_col, json_string(comment.c_str()));
			*out_size += comment.size();
		}
	}

	if (game_id >= TH135) {
		*out_size += 512; // Because these files are zipped, guessing their exact output size is hard. We'll add a few more bytes.
	}
	return ret;
}


json_t* bgm_generate_from_map(std::unordered_map<std::string, json_t*> in_data, const std::string out_fn, size_t* out_size)
{
	if (out_size) {
		*out_size = 0;
	}

	const char *spell_col = nullptr;

	// Find themes.js and the map file in the input files list
	auto themes_it = in_data.find("themes.js");
	auto map_it = std::find_if(in_data.begin(), in_data.end(), [](std::pair<const std::string, json_t*>& value) -> bool {
		int offset = value.first.length() - strlen(".map.jdiff");
		if (offset < 0) {
			return false;
		}
		return value.first.compare(offset, std::string::npos, ".map.jdiff") == 0;
	});
	if (themes_it == in_data.end() || map_it == in_data.end()) {
		return NULL;
	}
	json_t *themes = themes_it->second;
	json_t *themes_map = map_it->second;

	json_t *ret = json_object();
	size_t i;
	json_t *value;
	json_array_foreach(themes_map, i, value) {
		const char* theme_id = json_object_get_string(value,  "id");
		const char* line     = json_object_get_string(value,  "line");
		const char* col      = json_object_get_string(value,  "col");
		const char* theme    = json_object_get_string(themes, theme_id);
		if (!theme || !line || !col) {
			continue;
		}

		json_t *line_obj = json_object_get_create(ret, line, JSON_OBJECT);
		json_object_set_new(line_obj, col, json_string(theme));
		*out_size += strlen(theme);
	}

	return ret;
}
