/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Spell card patching via breakpoints.
  */

#include <thcrap.h>
#include "thcrap_tsa.h"
  
// Lookup cache
static json_t *spells = NULL;
static json_t *spellcomments = NULL;
static int cache_spell_id = 0;
static int cache_spell_id_real = 0;

int BP_spell_id(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	int *spell_id = (int*)json_object_get_register(bp_info, regs, "spell_id");
	int *spell_id_real = (int*)json_object_get_register(bp_info, regs, "spell_id_real");
	int *spell_rank = (int*)json_object_get_register(bp_info, regs, "spell_rank");
	// ----------

	if(spell_id) {
		cache_spell_id = *spell_id;
		cache_spell_id_real = cache_spell_id;
	}
	if(spell_id_real) {
		cache_spell_id_real = *spell_id_real;
	}
	if(spell_rank) {
		cache_spell_id = cache_spell_id_real - *spell_rank;
	}
	return 1;
}

int BP_spell_name(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	const char **spell_name = (const char**)json_object_get_register(bp_info, regs, "spell_name");
	json_t *cave_exec = json_object_get(bp_info, "cave_exec");
	// ----------

	// Other breakpoints
	// -----------------
	BP_spell_id(regs, bp_info);
	// -----------------

	if(spell_name && cache_spell_id_real >= cache_spell_id) {
		const char *new_name = NULL;
		int i = cache_spell_id_real;

		// Count down from the real number to the given number
		// until we find something
		do {
			new_name = json_string_value(json_object_get_numkey(spells, i));
		} while( (i-- > cache_spell_id) && i >= 0 && !new_name );

		if(new_name) {
			*spell_name = new_name;
			return !json_is_false(cave_exec);
		}
	}
	return 1;
}

int BP_spell_comment_line(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	const char **str = (const char**)json_object_get_register(bp_info, regs, "str");
	size_t comment_num = json_object_get_hex(bp_info, "comment_num");
	size_t line_num = json_object_get_hex(bp_info, "line_num");
	json_t *cave_exec = json_object_get(bp_info, "cave_exec");
	// ----------

	// Other breakpoints
	// -----------------
	BP_spell_id(regs, bp_info);
	// -----------------

	if(str && comment_num) {
		json_t *json_cmt = NULL;
		size_t i = cache_spell_id_real;

		size_t cmt_key_str_len = strlen("comment_") + 16 + 1;
		VLA(char, cmt_key_str, cmt_key_str_len);
		sprintf(cmt_key_str, "comment_%u", comment_num);

		// Count down from the real number to the given number
		// until we find something
		do {
			json_cmt = json_object_get_numkey(spellcomments, i);
			json_cmt = json_object_get(json_cmt, cmt_key_str);
		} while( (i-- > cache_spell_id) && !json_is_array(json_cmt) );

		if(json_is_array(json_cmt)) {
			*str = json_array_get_string_safe(json_cmt, line_num);
			return !json_is_false(cave_exec);
		}
	}
	return 1;
}

int patch_std(BYTE *msg_out, size_t size_out, size_t size_in, json_t *patch, json_t *format)
{
	// TODO: This could be much nicer once issue #5 has been dealt with.
	spells_exit();
	spells_init();
	return 0;
}

void spells_init()
{
	spells = stack_game_json_resolve("spells.js", NULL);
	spellcomments = stack_game_json_resolve("spellcomments.js", NULL);
}

void spells_exit()
{
	json_decref(spells);
	json_decref(spellcomments);
}
