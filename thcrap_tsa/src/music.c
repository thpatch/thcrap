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
	const char *game = json_object_get_string(runconfig_get(), "game");
	if(!game || !json_is_object(themes)) {
		return NULL;
	}
	{
		size_t key_len = strlen(game) + 1 + 2 + str_num_digits(track) + 1;
		VLA(char, key, key_len);
		sprintf(key, "%s_%02u", game, track);
		return json_object_get_string(themes, key);
	}
}

void music_title_print(const char **str, const char *format_id, size_t track)
{
	if(str) {
		const char *format = strings_get(format_id);
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
		json_t *cmt = json_object_get_numkey(musiccmt, cache_track);
		if(json_is_array(cmt)) {
			*str = json_array_get_string_safe(cmt, cache_cmt_line);

			// Resolve "@" to a music title format string
			if(!strcmp(*str, "@")) {
				music_title_print(str, format_id, cache_track);
			}
		}
	}
	return 1;
}

void music_init()
{
	themes = stack_json_resolve("themes.js", NULL);
	musiccmt = stack_game_json_resolve("musiccmt.js", NULL);
}

void music_exit()
{
	json_decref(themes);
	json_decref(musiccmt);
}
