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

size_t track_id_internal = 0;
int track_id_displayed = 0; // int because it's an argument for a printf() %d

const char* music_title_get(size_t track)
{
	const char *ret = NULL;
	const char *game = runconfig_game_get();
	if(game) {
		json_t *themes = jsondata_get("themes.js");
		size_t key_len = strlen(game) + 1 + 2 + str_num_digits(track) + 1;
		VLA(char, key, key_len);
		sprintf(key, "%s_%02u", game, track);
		ret = json_object_get_string(themes, key);
		VLA_FREE(key);
	}
	return ret;
}

void music_title_print(const char **str, const char *format_id, size_t track_id_internal, int track_id_displayed)
{
	if(str) {
		const char *format = json_string_value(strings_get(format_id));
		const char *title = music_title_get(track_id_internal);
		if(title) {
			if(format) {
				*str = strings_sprintf(0, format, track_id_displayed, title);
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
	json_t *track = json_object_get(bp_info, "track");
	// ----------
	if(track) {
		track_id_internal = json_immediate_value(track, regs) + 1;
		track_id_displayed = track_id_internal;

		// Work around TH10's Game Over theme being #8 in the Music
		// Room of trial versions, but #18 in the full version.
		if(game_id == TH10 && track_id_internal == 8 && game_is_trial()) {
			track_id_internal = 18;
		}
	}
	return (const char**)json_object_get_pointer(bp_info, regs, "str");
}

int BP_music_title(x86_reg_t *regs, json_t *bp_info)
{
	const char **str = BP_music_params(regs, bp_info);
	const char *format_id = json_object_get_string(bp_info, "format_id");
	music_title_print(str, format_id, track_id_internal, track_id_displayed);
	return 1;
}

int BP_music_cmt(x86_reg_t *regs, json_t *bp_info)
{
	static size_t cache_cmt_line = 0;

	// Parameters
	// ----------
	const char **str = BP_music_params(regs, bp_info);
	const char *format_id = json_object_get_string(bp_info, "format_id");
	json_t *line_num = json_object_get(bp_info, "line_num");
	// ----------
	if(line_num) {
		cache_cmt_line = json_immediate_value(line_num, regs);
	}
	if(str && *str) {
		json_t *musiccmt = jsondata_game_get("musiccmt.js");
		json_t *cmt = json_object_numkey_get(musiccmt, track_id_internal);
		if(json_is_array(cmt)) {
			const char* str_rep = json_array_get_string_safe(cmt, cache_cmt_line);
			// Resolve "@" to a music title format string
			if(!strcmp(str_rep, "@")) {
				music_title_print(str, format_id, track_id_internal, track_id_displayed);
			} else {
				*str = str_rep;
			}
		}
	}
	return 1;
}

void music_mod_init(void)
{
	jsondata_add("themes.js");
	jsondata_game_add("musiccmt.js");
}
