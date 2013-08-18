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

static json_t *musiccmt = NULL;
static size_t cache_track = 0;
static size_t cache_cmt_line = 0;

// Helper for generic parameters
int breakpoint_cache_param(size_t *target, const char *param, x86_reg_t *regs, json_t *bp_info)
{
	size_t *val;
	if(!regs || !bp_info || !param) {
		return -1;
	}
	val = (size_t*)json_object_get_register(bp_info, regs, param);
	if(val) {
		*target = *val;
	}
	return 1;
}

int BP_music_cmt(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	const char **str = (const char**)json_object_get_register(bp_info, regs, "str");
	// ----------
	breakpoint_cache_param(&cache_track, "track", regs, bp_info);
	breakpoint_cache_param(&cache_cmt_line, "line_num", regs, bp_info);

	if(str && *str) {
		json_t *cmt = json_object_get_numkey(musiccmt, cache_track + 1);
		if(json_is_array(cmt)) {
			*str = json_array_get_string_safe(cmt, cache_cmt_line);
		}
	}
	return 1;
}

void music_init()
{
	musiccmt = stack_game_json_resolve("musiccmt.js", NULL);
}

void music_exit()
{
	json_decref(musiccmt);
}
