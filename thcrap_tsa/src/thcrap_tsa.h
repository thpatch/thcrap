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
  *	BYTE* data
  *		Buffer to encrypt
  *
  *	size_t data_len
  *		Length of [data]
  *
  *	const BYTE* params
  *		Optional array of parameters required
  *
  *	const BYTE* params_count
  *		Number of parameters in [params].
  *
  * Returns nothing.
  */

typedef void (*EncryptionFunc_t)(
	BYTE *data, size_t data_len, const BYTE *params, size_t params_count
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
  * For now, this is merely a dummy hook to make sure that the spell files are
  * reloaded at the beginning of a stage.
  * We don't want to hook ECL files because newer games use more than one per
  * stage, and their file names are not really predictable anymore.
  * We also don't just do this once on plug-in initialization because the files
  * may be changed by an automatic update at any time.
  */
int patch_std(BYTE *msg_out, size_t size_out, size_t size_in, json_t *patch, json_t *run_cfg);

void spells_init();
void spells_exit();
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

void music_init();
void music_exit();
/// ----------

/// Format patchers
/// ---------------
int patch_msg(BYTE *msg_out, size_t size_out, size_t size_in, json_t *patch, json_t *format);
int patch_msg_dlg(BYTE *msg_out, size_t size_out, size_t size_in, json_t *patch, json_t *run_cfg);
int patch_msg_end(BYTE *msg_out, size_t size_out, size_t size_in, json_t *patch, json_t *run_cfg);
int patch_anm(BYTE *msg_out, size_t size_out, size_t size_in, json_t *patch, json_t *run_cfg);
/// ---------------

/// Win32 wrappers
/// --------------
int tsa_detour(HMODULE hMod);
/// --------------
