/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Main include file.
  */

#pragma once

typedef enum {
	TH_NONE,

	// • msg: Hard lines only
	TH06,
	TH07,

	// • msg: Text starts being encrypted with a simple XOR
	// • msg: Automatic lines mostly, hard lines for effect
	TH08,

	// • msg: Hard lines stop being used
	// • msg: Text starts being encrypted with a more complicated XOR using
	//        two step variables
	TH09,

	// • Introduces a custom format for mission.msg
	TH095,

	// • msg: Different opcodes
	// • end: Now uses the regular MSG format with a different set of opcodes
	TH10,
	ALCOSTG,

	// • anm: Header structure is changed
	TH11,
	TH12,

	// • mission: Adds ruby support
	TH125,

	// • Changes the fixed-size text boxes to variable-size speech bubbles
	TH128,

	// • Changes the speech bubble shape
	TH13,

	// • Changes the speech bubble shape
	// • msg: Adds opcode 32 for overriding the speech bubble shape and
	//        direction
	TH14,

	// Any future game without relevant changes
	TH_FUTURE,
} tsa_game_t;

extern tsa_game_t game_id;

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

void music_title_print(const char **str, const char *format_id, size_t track_id_internal, int track_id_displayed);

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
int patch_msg_dlg(uint8_t *file_inout, size_t size_out, size_t size_in, json_t *patch);
int patch_msg_end(uint8_t *file_inout, size_t size_out, size_t size_in, json_t *patch);
int patch_anm(uint8_t *file_inout, size_t size_out, size_t size_in, json_t *patch);
/// ---------------

/// Win32 wrappers
/// --------------
void tsa_mod_detour(void);
/// --------------
