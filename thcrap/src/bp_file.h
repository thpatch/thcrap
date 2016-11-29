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

// File replacement state
typedef struct {
	// Combined JSON patch to be applied to the file, and its maximum size
	json_t *patch;
	size_t patch_size;
	// JSON array of hook functions to be run on this file
	json_t *hooks;

	// File name. Enforced to be in UTF-8
	char *name;

	// Potential replacement file, applied before the JSON patch
	void *rep_buffer;
	// Size of [rep_buffer] if we have one; otherwise, size of the original
	// game file. [game_buffer] is guaranteed to be at least this large.
	size_t pre_json_size;

	void *game_buffer;

	// Pointer to an object of the game's file class. Used as another
	// method of linking load calls to files, if file names aren't enough.
	void *object;
} file_rep_t;

// Initialize a file_rep_t object, and loads the replacement file and patch for file_name.
int file_rep_init(file_rep_t *fr, const char *file_name);

// Clears a file_rep_t object.
int file_rep_clear(file_rep_t *fr);

/// Thread-local storage
/// --------------------
file_rep_t* fr_tls_get(void);
void fr_tls_free(file_rep_t *fr);
/// --------------------

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
  * Other breakpoints called
  * ------------------------
  *	BP_file_name
  */
int BP_file_size(x86_reg_t *regs, json_t *bp_info);

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
  * function call, after the final buffer has been allocated. Thus, it allows
  * manipulation of ESP and EIP, if necessary.
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

int bp_file_init(void);
void bp_file_mod_thread_exit(void);
int bp_file_exit(void);
