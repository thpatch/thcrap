/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * VFS generators for spellcard files
  */

#include <thcrap.h>
#include <vfs.h>
#include "spellcards_generator.h"

json_t* spell_story_generator(std::unordered_map<std::string, json_t*> in_data, const std::string out_fn, size_t* out_size)
{
	if (out_size) {
		*out_size = 0;
	}

	// Extract the player and spell id from the file name
	const char *out_fn_ptr = strchr(out_fn.c_str(), '/');
	if (out_fn_ptr == NULL || strncmp(out_fn_ptr, "/data/csv/story/", strlen("/data/csv/story/")) != 0) {
		return NULL;
	}
	out_fn_ptr += strlen("/data/csv/story/");
	const char *character_name_end = strchr(out_fn_ptr, '/');
	if (character_name_end == nullptr) {
		return NULL;
	}
	VLA(char, character_name, character_name_end - out_fn_ptr + 1);
	strncpy(character_name, out_fn_ptr, character_name_end - out_fn_ptr);
	character_name[character_name_end - out_fn_ptr] = '\0';

	out_fn_ptr = character_name_end + strlen("/stage");
	int stage_num = atoi(out_fn_ptr);
	if (strcmp(out_fn_ptr + 1, ".csv.jdiff") != 0) {
		VLA_FREE(character_name);
		return NULL;
	}

	// Find spells.js in the input files list
	char* spells_fn = fn_for_game("spells.js");
	auto spells_it = in_data.find(spells_fn);
	SAFE_FREE(spells_fn);
	if (spells_it == in_data.end()) {
		VLA_FREE(character_name);
		return NULL;
	}
	json_t *spells = spells_it->second;

	// Build the json object from the spellcards list.
	json_t *ret = json_object();
	const char *key;
	json_t *value;
	json_object_foreach(spells, key, value) {
		if (strncmp(key, "story-", strlen("story-")) != 0) {
			continue;
		}
		const char *key_ptr = key + strlen("story-");
		if (strncmp(key_ptr, character_name, strlen(character_name)) != 0) {
			continue;
		}
		key_ptr += strlen(character_name);
		if (strncmp(key_ptr, "-stage", strlen("-stage")) != 0) {
			continue;
		}
		key_ptr += strlen("-stage");
		if (atoi(key_ptr) != stage_num) {
			continue;
		}
		if (json_is_string(value) == false) {
			continue;
		}
		key_ptr += 2;
		int spell_id = atoi(key_ptr);
		key_ptr += 2;
		int difficulty_id = atoi(key_ptr);

		// A max spell id of 97 is more than enough, and the difficulty should be between 1 and 4.
		char ret_key[3];
		sprintf(ret_key, "%d", (spell_id + 2) % 100);
		json_t *col = json_object_get_create(ret, ret_key, JSON_OBJECT);
		sprintf(ret_key, "%d", (difficulty_id + 8) % 100);
		json_object_set(col, ret_key, value);
		*out_size += json_string_length(value);
	}

	*out_size += 512; // Because these files are zipped, guessing their exact output size is hard. We'll add a few more bytes.
	VLA_FREE(character_name);
	if (json_object_size(ret) == 0) {
		json_decref(ret);
		ret = NULL;
	}
	return ret;
}

json_t* spell_player_generator(std::unordered_map<std::string, json_t*> in_data, const std::string out_fn, size_t* out_size)
{
	if (out_size) {
		*out_size = 0;
	}

	// Extract the player and spell id from the file name
	const char *out_fn_ptr = strchr(out_fn.c_str(), '/');
	if (out_fn_ptr == NULL || strncmp(out_fn_ptr, "/data/csv/spellcard/", strlen("/data/csv/spellcard/")) != 0) {
		return NULL;
	}
	out_fn_ptr += strlen("/data/csv/spellcard/");
	if (strchr(out_fn_ptr, '.') == NULL || strcmp(strchr(out_fn_ptr, '.'), ".csv.jdiff") != 0) {
		return NULL;
	}
	VLA(char, character_name, strlen(out_fn_ptr) + 1);
	strcpy(character_name, out_fn_ptr);
	*strchr(character_name, '.') = '\0';

	// Find spells.js in the input files list
	char* spells_fn = fn_for_game("spells.js");
	auto spells_it = in_data.find(spells_fn);
	SAFE_FREE(spells_fn);
	if (spells_it == in_data.end()) {
		VLA_FREE(character_name);
		return NULL;
	}
	json_t *spells = spells_it->second;

	// Build the json object from the spellcards list.
	json_t *ret = json_object();
	const char *key;
	json_t *value;
	json_object_foreach(spells, key, value) {
		if (strncmp(key, "player-", strlen("player-")) != 0) {
			continue;
		}
		const char *key_ptr = strchr(key, '(') + 1;
		if (strncmp(key_ptr, character_name, strlen(character_name)) != 0) {
			continue;
		}
		key_ptr = strchr(key_ptr, '-');
		if (!key_ptr) {
			continue;
		}
		int spell_id = atoi(key_ptr + 1);
		if (json_is_string(value) == false) {
			continue;
		}

		// A max spell id of 98 is more than enough.
		char ret_key[3];
		sprintf(ret_key, "%d", (spell_id + 1) % 100);
		json_t *col = json_object();
		json_object_set(col, "2", value);
		json_object_set_new(ret, ret_key, col);
		*out_size += json_string_length(value);
	}

	*out_size += 512; // Because these files are zipped, guessing their exact output size is hard. We'll add a few more bytes.
	VLA_FREE(character_name);
	if (json_object_size(ret) == 0) {
		json_decref(ret);
		ret = NULL;
	}
	return ret;
}

json_t* spell_char_select_generator(std::unordered_map<std::string, json_t*> in_data, const std::string out_fn, size_t* out_size)
{
	if (out_size) {
		*out_size = 0;
	}

	// Extract the player and spell id from the file name
	const char *out_fn_ptr = strchr(out_fn.c_str(), '/');
	if (out_fn_ptr == NULL || strncmp(out_fn_ptr, "/data/system/char_select3/", strlen("/data/system/char_select3/")) != 0) {
		return NULL;
	}
	out_fn_ptr += strlen("/data/system/char_select3/");
	int character_id = atoi(out_fn_ptr) + 1;
	out_fn_ptr = strchr(out_fn_ptr, '/');
	if (out_fn_ptr == NULL || strncmp(out_fn_ptr, "/equip/", strlen("/equip/")) != 0) {
		return NULL;
	}
	int spell_id = out_fn_ptr[strlen("/equip/")] - 'a' + 1;

	// Find spells.js in the input files list
	char* spells_fn = fn_for_game("spells.js");
	auto spells_it = in_data.find(spells_fn);
	SAFE_FREE(spells_fn);
	if (spells_it == in_data.end()) {
		return NULL;
	}
	json_t *spells = spells_it->second;

	// Search the spellcard in the spellcards list.
	json_t *spell_translation = NULL;
	const char *key;
	json_t *value;
	json_object_foreach(spells, key, value) {
		if (strncmp(key, "player-", strlen("player-")) != 0) {
			continue;
		}
		const char *key_ptr = key + strlen("player-");
		if (atoi(key_ptr) != character_id) {
			continue;
		}
		key_ptr = strchr(key_ptr, ')') + 2;
		if (atoi(key_ptr) != spell_id) {
			continue;
		}
		if (json_is_string(value) == false) {
			continue;
		}
		spell_translation = value;
		json_incref(spell_translation);
		break;
	}
	if (spell_translation == NULL) {
		return NULL;
	}

	*out_size = json_string_length(spell_translation);
	json_t *ret = json_pack("{s{ss}}", "0", "5", json_string_value(spell_translation));
	json_decref(spell_translation);
	return ret;
}
