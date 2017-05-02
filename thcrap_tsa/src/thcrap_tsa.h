/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Main include file.
  */

#pragma once

/**
  * Encryption function type.
  *
  * Parameters
  * ----------
  *	uint8_t* data
  *		Buffer to encrypt
  *
  *	size_t data_len
  *		Length of [data]
  *
  *	const uint8_t* params
  *		Optional array of parameters required
  *
  *	const uint8_t* params_count
  *		Number of parameters in [params].
  *
  * Returns nothing.
  */

typedef void (*EncryptionFunc_t)(
	uint8_t *data, size_t data_len, const uint8_t *params, size_t params_count
);

/// ------
/// Spells
/// ------
/**
  * Reads a spell card ID.
  *
  * Own JSON parameters
  * -------------------
  *	[spell_id]
  *		The ID as given in the ECL file.
  *		Used as the minimum value for the name search.
  *		Type: register
  *
  *	[spell_id_real]
  *		The ID with the difficulty offset added to it.
  *		Used as the maximum value for the name search.
  *		Type: register
  *
  *	[spell_rank]
  *		The difficulty level ID offset.
  *		Used to calculate [spell_id] from [spell_id_real]
  *		if that parameter not available.
  *		Type: register
  *
  * Other breakpoints called
  * ------------------------
  *	None
  */
int BP_spell_id(x86_reg_t *regs, json_t *bp_info);

/**
  * Writes a translated spell card title.
  *
  * Own JSON parameters
  * -------------------
  *	[spell_name]
  *		Register to write to.
  *		Type: register
  *
  *	[cave_exec]
  *		Set to false to disable the execution of the code cave
  *		if the spell name is replaced.
  *		Type: bool
  *
  * Other breakpoints called
  * ------------------------
  *	BP_spell_id
  */
int BP_spell_name(x86_reg_t *regs, json_t *bp_info);

/**
  * Writes a single translated spell comment line.
  *
  * Own JSON parameters
  * -------------------
  *	[str]
  *		Register to write to.
  *		Type: register
  *
  *	[comment_num]
  *		Comment number. Gets prefixed with "comment_" to make up the JSON key.
  *		Must be at least 1.
  *		Type: hex
  *
  *	[line_num]
  *		Line number. Array index in the JSON array of [comment_num].
  *		Type: hex
  *
  *	[cave_exec]
  *		Set to false to disable the execution of the code cave
  *		if the spell name is replaced.
  *		Type: bool
  *
  * Other breakpoints called
  * ------------------------
  *	BP_spell_id
  */
int BP_spell_comment_line(x86_reg_t *regs, json_t *bp_info);

/**
* Writes a translated spell card owner name.
*
* Own JSON parameters
* -------------------
*	[spell_owner]
*		Register to write to.
*		Type: register
*
*	[cave_exec]
*		Set to false to disable the execution of the code cave
*		if the spell owner name is replaced.
*		Type: bool
*
* Other breakpoints called
* ------------------------
*	BP_spell_id
*/
int BP_spell_owner(x86_reg_t *regs, json_t *bp_info);

void spells_mod_init(void);
void spells_mod_exit(void);
/// ------

/// Music Room
/// ----------
// Returns the translated title for track #[track] of the current game.
const char* music_title_get(size_t track);

// Prints the translated title of track #[track] to [str], according to
// [format_id] in the string definition table. This format string receives
// the track number and its translated title, in this order.
void music_title_print(const char **str, const char *format_id, size_t track);

/**
  * Pseudo breakpoint taking parameters cached across the other
  * Music Room breakpoints. *Not* exported.
  *
  * Own JSON parameters
  * -------------------
  *	[track]
  *		Music Room track number, 0-based. Gets cached 1-based.
  *		Type: register
  *
  *	[str]
  *		Register containing the address of the target string.
  *		Also the return value of this function.
  *		Type: register
  */
const char** BP_music_params(x86_reg_t *regs, json_t *bp_info);

/**
  * Music title patching.
  *
  * Own JSON parameters
  * -------------------
  * [format_id]
  *		Music title format string to use for the title.
  *		Type: string (ID in the string definition table)
  *
  * Other breakpoints called
  * ------------------------
  *	BP_music_params
  */
int BP_music_cmt(x86_reg_t *regs, json_t *bp_info);

/**
  * Music comment patching.
  *
  * Own JSON parameters
  * -------------------
  * [format_id]
  *		Music title format string to replace lines consisting of a single "@".
  *		Type: string (ID in the string definition table)
  *
  *	[line_num]
  *		Line number of the comment.
  *		Type: register
  *
  * Other breakpoints called
  * ------------------------
  *	BP_music_params
  */
int BP_music_cmt(x86_reg_t *regs, json_t *bp_info);

void music_mod_init(void);
void music_mod_exit(void);
/// ----------

/// Format patchers
/// ---------------
int patch_msg(uint8_t *file_inout, size_t size_out, size_t size_in, json_t *patch, json_t *format);
int patch_msg_dlg(uint8_t *file_inout, size_t size_out, size_t size_in, json_t *patch);
int patch_msg_end(uint8_t *file_inout, size_t size_out, size_t size_in, json_t *patch);
int patch_anm(uint8_t *file_inout, size_t size_out, size_t size_in, json_t *patch);
/// ---------------

/// Win32 wrappers
/// --------------
void tsa_mod_detour(void);
/// --------------
