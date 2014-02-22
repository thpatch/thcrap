/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Music Room patching.
  */

#include <thcrap.h>
#include "thcrap_tsa.h"

static json_t *themes = NULL;
static json_t *musiccmt = NULL;
static size_t cache_track = 0;

const char* music_title_get(size_t track)
{
	const char *ret = NULL;
	json_t *game = json_object_get(runconfig_get(), "game");
	if(json_is_string(game)) {
		size_t key_len = json_string_length(game) + 1 + 2 + str_num_digits(track) + 1;
		VLA(char, key, key_len);
		sprintf(key, "%s_%02u", json_string_value(game), track);
		ret = json_object_get_string(themes, key);
		VLA_FREE(key);
	}
	return ret;
}

void music_title_print(const char **str, const char *format_id, size_t track)
{
	if(str) {
		const char *format = strings_get_value(format_id);
		const char *title = music_title_get(track);
		if(title) {
			if(format) {
				*str = strings_sprintf(0, format, track, title);
			} else {
				*str = title;
			}
		}
	}
}

const char** BP_music_params(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	size_t *track = (size_t*)json_object_get_register(bp_info, regs, "track");
	// ----------
	if(track) {
		cache_track = (*track) + 1;
	}
	return (const char**)json_object_get_register(bp_info, regs, "str");
}

int BP_music_title(x86_reg_t *regs, json_t *bp_info)
{
	const char **str = BP_music_params(regs, bp_info);
	const char *format_id = json_object_get_string(bp_info, "format_id");
	music_title_print(str, format_id, cache_track);
	return 1;
}

int BP_music_cmt(x86_reg_t *regs, json_t *bp_info)
{
	static size_t cache_cmt_line = 0;

	// Parameters
	// ----------
	const char **str = BP_music_params(regs, bp_info);
	const char *format_id = json_object_get_string(bp_info, "format_id");
	size_t *line_num = json_object_get_register(bp_info, regs, "line_num");
	// ----------
	if(line_num) {
		cache_cmt_line = *line_num;
	}
	if(str && *str) {
		json_t *cmt = json_object_numkey_get(musiccmt, cache_track);
		if(json_is_array(cmt)) {
			const char* str_rep = json_array_get_string_safe(cmt, cache_cmt_line);
			// Resolve "@" to a music title format string
			if(!strcmp(str_rep, "@")) {
				music_title_print(str, format_id, cache_track);
			} else {
				*str = str_rep;
			}
		}
	}
	return 1;
}

void music_mod_init(void)
{
	themes = stack_json_resolve("themes.js", NULL);
	musiccmt = stack_game_json_resolve("musiccmt.js", NULL);
}

void music_mod_exit(void)
{
	themes = json_decref_safe(themes);
	musiccmt = json_decref_safe(musiccmt);
}
