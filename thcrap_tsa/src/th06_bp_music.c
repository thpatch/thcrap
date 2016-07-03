/**
* Touhou Community Reliant Automatic Patcher
* Team Shanghai Alice support plugin
*
* ----
*
* Th06-specific breakpoint for in-game music titles.
*/

#include <thcrap.h>
#include <bp_file.h>
#include "thcrap_tsa.h"

int BP_th06_music_title_in_game(x86_reg_t *regs, json_t *bp_info)
{
	static size_t track = 0;

	// Parameters
	// ----------
	size_t *stage = (size_t*)json_object_get_register(bp_info, regs, "stage");
	int offset = json_object_get_hex(bp_info, "offset");
	const char **str = (const char**)json_object_get_register(bp_info, regs, "str");
	// ----------

	if (stage) {
		track = *stage * 2;
	}
	if (str && *str) {
		music_title_print(str, NULL, track + offset);
	}
	return 1;
}