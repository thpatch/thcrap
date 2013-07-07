/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoints for contiguous files.
  *
  * These breakpoint can be used for replacing data files in any game that
  * loads complete data files into a single block of memory.
  *
  * Note that parameters are always passed through to other breakpoint
  * functions called by a function.
  */

#pragma once

#define HAVE_BP_FILE 1

// File replacement state
typedef struct {
	// Combined JSON patch that will be applied to the file
	json_t *patch;
	// JSON array of hook functions to be run on this file
	json_t *hooks;

	// File name. Enforced to be in UTF-8
	char *name;

	// Size of the thing we're replacing the game's file with
	size_t local_size;
	// Size of the game's own file in the game's own memory
	size_t game_size;

	void *local_buffer;
	void *game_buffer;

	// Pointer to an object of the game's file class.
	// Used as another method of linking load calls to files, if file names aren't enough
	void *object;
} file_rep_t;

/**
  * Takes a file name and resolves a complete local replacement
  * or JSON patch file.
  *
  * Own JSON parameters
  * -------------------
  *	[file_name]
  *		File name register
  *		Type: register
  *
  * Other breakpoints called
  * ------------------------
  *	None
  */
int BP_file_name(x86_reg_t *regs, json_t *bp_info);

/**
  * Reads the size of the original file, and sets the size of whatever we are
  * replacing or patching the original file with.
  *
  * Own JSON parameters
  * -------------------
  *	[file_size]
  *		Register that contains the size of the file to load.
  *		Type: register
  *
  *	[set_patch_size]
  *		Set to false to not write the size of the patched file
  *		to the [file_size] register.
  *		Type: bool
  *
  * Other breakpoints called
  * ------------------------
  *	BP_file_name
  */
int BP_file_size(x86_reg_t *regs, json_t *bp_info);

/**
  * If we define a larger buffer in [BP_file_size], the game's decompression
  * algorithm may produce bogus data after the end of the original file.
  * This breakpoint can be used to write the actual size of the completely
  * patched file separately from [BP_file_size].
  *
  * Own JSON parameters
  * -------------------
  *	[file_size]
  *		Register to write the patch size to.
  *		Type: register
  *
  * Other breakpoints called
  * ------------------------
  *	None
  */
int BP_file_size_patch(x86_reg_t *regs, json_t *bp_info);

/**
  * Reads the file buffer address and stores it in the local file_rep_t object.
  *
  * Own JSON parameters
  * -------------------
  *	[file_buffer]
  *		Register containing the address of the file buffer
  *		Type: register
  *
  * Other breakpoints called
  * ------------------------
  *	None
  */
int BP_file_buffer(x86_reg_t *regs, json_t *bp_info);

/**
  * Writes a local replacement file to the game's file buffer, and also
  * applies a JSON patch on top of it if available.
  * This breakpoint is mostly placed at the game's own "load-file-into-buffer"
  * function call. Thus, it allows for manipulation of ESP and EIP,
  * if necessary.
  * Returns 0 if anything was replaced (to skip execution of the original
  * function in the breakpoint's code cave).
  *
  * Own JSON parameters
  * -------------------
  *	[file_buffer_addr_copy]
  *		Additional register to copy the file buffer to (optional)
  *		Type: register
  *
  *	[stack_clear_size]
  *		Stack size to clean up (optional)
  *		Type: integer
  *
  *	[eip_jump_dist]
  *		Value to add to EIP after this breakpoint (optional)
  *		Type: integer
  *
  * Other breakpoints called
  * ------------------------
  *	BP_file_buffer
  */
int BP_file_load(x86_reg_t *regs, json_t *bp_info);

/**
  * Placed at a position where a complete file is loaded into a single block
  * of memory, this breakpoint...
  * - applies a JSON patch on top of it if available
  * - dumps the original file (unless [dat_dump] in the run configuration is false)
  *
  * Own JSON parameters
  * -------------------
  *	None
  *
  * Other breakpoints called
  * ------------------------
  *	BP_file_buffer
  */
int BP_file_loaded(x86_reg_t *regs, json_t *bp_info);

/**
  * Clears a file_rep_t object.
  */
int file_rep_clear(file_rep_t *pf);

int bp_file_init();
