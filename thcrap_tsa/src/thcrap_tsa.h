/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Main include file.
  */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	TH_NONE,

	// • msg: Hard lines only
	// • bgm: Uses the MMIO API with separate .wav files, together with
	//        separate .pos files to define loop points
	TH06,

	// • bgm: Early trial versions use .wav.sli files to define loop points
	// • bgm: Full version switches to thbgm.dat, read using the KERNEL32
	//        file API, indexed by the new .fmt files
	TH07,

	// • msg: Text starts being encrypted with a simple XOR
	// • msg: Automatic lines mostly, hard lines for effect
	TH08,

	// • msg: Hard lines stop being used
	// • msg: Text starts being encrypted with a more complicated XOR using
	//        two step variables
	TH09,

	// • Introduces a custom format for mission.msg
	// • Fonts start being created once at the beginning of the game, rather
	//   than for every line of text rendered
	TH095,

	// • msg: Different opcodes
	// • end: Now uses the regular MSG format with a different set of opcodes
	TH10,
	ALCOSTG,

	// • anm: Header structure is changed, introducing support for X and Y
	//        offsets
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
	TH143,
	TH15,

	// • First game released on Steam
	TH16,
	TH165,
	TH17,

	// Any future game without relevant changes
	TH_FUTURE,
} tsa_game_t;

#ifdef __cplusplus
// Define iteration over all games via `for(const  auto &game : tsa_game_t())`
tsa_game_t operator ++(tsa_game_t &game);
tsa_game_t operator *(tsa_game_t game);
tsa_game_t begin(tsa_game_t game);
tsa_game_t end(tsa_game_t game);
#endif

extern tsa_game_t game_id;

// Returns 0 if the currently running game is a full version, 1 if it is a
// trial, or -1 if this can't be determined.
int game_is_trial(void);

/// BGM modding
/// -----------
/*
 * Separately records the byte position within the next track that thbgm.dat
 * will be seeked to, to make sure that the correct track can be identified
 * regardless of the length of the modded BGM.
 * Ctrl-F "trance seeking" in bgm.cpp for why this is necessary for TH13.
 *
 * Own JSON parameters
 * -------------------
 *	[offset]
 *		Pointer to the number of bytes after the beginning of the track
 *		Type: immediate
 *
 * Other breakpoints called
 * ------------------------
 *	None
 */
__declspec(dllexport) int BP_bgmmod_tranceseek_byte_offset(x86_reg_t *regs, json_t *bp_info);
/// -----------

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
  *		Type: immediate
  *
  *	[spell_id_real]
  *		The ID with the difficulty offset added to it.
  *		Used as the maximum value for the name search.
  *		Type: immediate
  *
  *	[spell_rank]
  *		The difficulty level ID offset.
  *		Used to calculate [spell_id] from [spell_id_real]
  *		if that parameter not available.
  *		Type: immediate
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
  *		Address to write to.
  *		Type: pointer
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
  *		Type: immediate
  *
  *	[str]
  *		Address of the target string.
  *		Also the return value of this function.
  *		Type: pointer
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
  *		Type: immediate
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
int patch_msg_dlg(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch);
int patch_msg_end(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch);
int patch_end_th06(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch);
int patch_anm(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch);
/// ---------------

/// Win32 wrappers
/// --------------
void tsa_mod_detour(void);
/// --------------

#ifdef __cplusplus
}
#endif
