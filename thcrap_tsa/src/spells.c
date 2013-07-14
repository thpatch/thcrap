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
static size_t cache_spell_id;
static size_t cache_spell_id_real;

int BP_spell_id(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	size_t *spell_id = json_object_get_register(bp_info, regs, "spell_id");
	size_t *spell_id_real = json_object_get_register(bp_info, regs, "spell_id_real");
	// ----------

	if(spell_id) {
		cache_spell_id = *spell_id;
		cache_spell_id_real = cache_spell_id;
	}
	if(spell_id_real) {
		cache_spell_id_real = *spell_id_real;
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

	if(spell_name) {
		const char *new_name = NULL;
		size_t i = cache_spell_id_real;

		// Count down from the real number to the given number
		// until we find something
		do {
			char key_str[16];
			_itoa(i, key_str, 10);
			new_name = json_object_get_string(spells, key_str);
		} while( (i-- > cache_spell_id) && !new_name );

		if(new_name) {
			*spell_name = new_name;
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
}

void spells_exit()
{
	json_decref(spells);
}
