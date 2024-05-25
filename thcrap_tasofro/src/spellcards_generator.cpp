/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * VFS generators for spellcard files
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"
#include <vfs.h>
#include "spellcards_generator.h"

json_t* spell_story_generator(std::unordered_map<std::string_view, json_t*>& in_data, std::string_view out_fn, size_t& out_size)
{
	// Extract the player and spell id from the file name
	const char *out_fn_ptr = strchr(out_fn.data(), '/');
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

	// Get the max stage id.
	const char *key;
	json_t *value;
	int max_spell_id = -1;
	size_t character_name_len = strlen(character_name);
	json_object_foreach_fast(spells, key, value) {
		if (strncmp(key, "story-", strlen("story-")) != 0) {
			continue;
		}
		const char *key_ptr = key + strlen("story-");
		if (strncmp(key_ptr, character_name, character_name_len) != 0) {
			continue;
		}
		key_ptr += character_name_len;
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
		if (spell_id > max_spell_id) {
			max_spell_id = spell_id;
		}
	}

	json_t *ret = nullptr;
	if (max_spell_id != -1) {
		DWORD story_spell_col;
		if (game_id >= TH145) {
			story_spell_col = 8;
		}
		else {
			story_spell_col = 10;
		}
		// Build the json object.
		ret = json_object();
		VLA(char, spell_name, 6 /* story- */ + character_name_len + story_spell_col /* -stageNN */ + 3 /* -NN */ + 2 /* -N */ + 1);
		for (int spell_id = 1; spell_id <= max_spell_id; spell_id++) {
			const char *last_spell_translation = nullptr;
			for (int difficulty_id = 1; difficulty_id <= 4; difficulty_id++) {

				sprintf(spell_name, "story-%s-stage%d-%d-%d", character_name, stage_num % 100, spell_id % 100, difficulty_id % 10);
				const char *spell_translation = json_object_get_string(spells, spell_name);
				if (!spell_translation) {
					spell_translation = last_spell_translation;
				}
				if (spell_translation) {
					char key[3];
					sprintf(key, "%d", (spell_id + 1) % 100);
					json_t *col = json_object_get_create(ret, key, JSON_OBJECT);
					sprintf(key, "%d", (difficulty_id + 8) % 100);
					json_object_set_new(col, key, json_string(spell_translation));
					out_size += strlen(spell_translation);
					last_spell_translation = spell_translation;
				}
			}
		}
		VLA_FREE(spell_name);
	}

	VLA_FREE(character_name);
	return ret;
}

json_t* spell_player_generator(std::unordered_map<std::string_view, json_t*>& in_data, std::string_view out_fn, size_t& out_size)
{
	// Extract the player and spell id from the file name
	const char *out_fn_ptr = strchr(out_fn.data(), '/');
	if (out_fn_ptr == NULL) {
		return NULL;
	}
	if (strncmp(out_fn_ptr, "/data/csv/spellcard/", strlen("/data/csv/spellcard/")) == 0) {
		out_fn_ptr += strlen("/data/csv/spellcard/");
	}
	else {
		return NULL;
	}
	const char* extension = strchr(out_fn_ptr, '.');
	if (extension == NULL || strcmp(extension, ".csv.jdiff") != 0) {
		return NULL;
	}
	size_t character_name_len = PtrDiffStrlen(extension, out_fn_ptr);
	VLA(char, character_name, character_name_len + 1);
	character_name[character_name_len] = '\0';
	memcpy(character_name, out_fn_ptr, character_name_len);
	character_name[0] = tolower(character_name[0]);

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
	json_object_foreach_fast(spells, key, value) {
		if (strncmp(key, "player-", strlen("player-")) != 0) {
			continue;
		}
		const char *key_ptr = key + strlen("player-");
		if (strncmp(key_ptr, character_name, character_name_len) != 0) {
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
		out_size += json_string_length(value);
	}

	VLA_FREE(character_name);
	if (json_object_size(ret) == 0) {
		json_decref(ret);
		ret = NULL;
	}
	return ret;
}

json_t* spell_char_select_generator(std::unordered_map<std::string_view, json_t*>& in_data, std::string_view out_fn, size_t& out_size)
{
	// Extract the player and spell id from the file name
	const char *out_fn_ptr = strchr(out_fn.data(), '/');
	if (out_fn_ptr == NULL || strncmp(out_fn_ptr, "/data/system/char_select3/", strlen("/data/system/char_select3/")) != 0) {
		return NULL;
	}
	out_fn_ptr += strlen("/data/system/char_select3/");
	int character_id = atoi(out_fn_ptr);
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
	json_object_foreach_fast(spells, key, value) {
		if (strncmp(key, "player-", strlen("player-")) != 0) {
			continue;
		}
		const char *key_ptr = strchr(key, '(');
		if (key_ptr == nullptr || atoi(key_ptr + 1) != character_id) {
			continue;
		}
		key_ptr = strchr(key_ptr, ')');
		if (key_ptr == nullptr || atoi(key_ptr + 2) != spell_id) {
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

	out_size = json_string_length(spell_translation) * 2; // Escaped characters take more place
	json_t *ret = json_pack("{s{ss}}", "0", "5", json_string_value(spell_translation));
	json_decref(spell_translation);
	return ret;
}
