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
  */

#pragma once

// File replacement state
typedef struct {
	// Combined JSON patch to be applied to the file, and its maximum size
	json_t *patch;
	size_t patch_size;
	// JSON array of hook functions to be run on this file
	struct patchhook_t *hooks;

	// File name. Enforced to be in UTF-8
	char *name;

	// Potential replacement file, applied before the JSON patch
	void *rep_buffer;
	// Size of [rep_buffer] if we have one; otherwise, size of the original
	// game file. [game_buffer] is guaranteed to be at least this large.
	size_t pre_json_size;

	void *game_buffer;

	// A hack to be able to temporairly disable file patching on the current thread
	// if something that runs as part of the file patcher needs to load a file
	bool disable;
} file_rep_t;

// Initialize a file_rep_t object, and loads the replacement file and patch for file_name.
THCRAP_API int file_rep_init(file_rep_t *fr, const char *file_name);

// Clears a file_rep_t object.
THCRAP_API int file_rep_clear(file_rep_t *fr);

// Return the size of the buffer needed for the patched file
#define POST_JSON_SIZE(fr) ((fr)->pre_json_size + (fr)->patch_size)


/// Thread-local storage
/// --------------------
THCRAP_API file_rep_t* fr_tls_get(void);
void fr_tls_free(file_rep_t *fr);
/// --------------------

/**
  * Reads the file buffer address and stores it in the local file_rep_t object.
  *
  * Own JSON parameters
  * -------------------
  *	[file_buffer]
  *		Register containing the address of the file buffer
  *		Type: pointer
  *
  * Other breakpoints called
  * ------------------------
  *	None
  */
THCRAP_BREAKPOINT_API int BP_file_buffer(x86_reg_t *regs, json_t *bp_info);

/**
  * Collects all necessary file replacement parameters, then writes a
  * potential replacement file to the game's file buffer, and also
  * applies a potential JSON patch on top of it.
  * This breakpoint is mostly placed at the game's own "load-file-into-buffer"
  * function call, after the final buffer has been allocated.
  *
  * Returns:
  * • 0 if the file was fully replaced, skipping execution of the
  *   breakpoint's code cave. Since we're done with the file at
  *   this point, the file replacement state is cleared in that case.
  * • 1 if either some of the mandatory parameters are missing, or if
  *   there is no full replacement file in the stack. A later call to
  *   BP_file_loaded() should then apply a potential JSON patch on top
  *   of the original file.
  *
  * Own JSON parameters
  * -------------------
  * Mandatory:
  *
  *	[file_name]
  *		If given, initializes the file replacement state with the
  *		given file name, resolving a corresponding complete
  *		replacement file and/or a JSON patch from the stack.
  *		Type: pointer
  *
  *	[file_size]
  *		If given, reads the size of the original file (if not
  *		read before), and (always) sets the final size of whatever
  *		we are replacing or patching the original file with.
  *		Necessary for enlarging the size of the game's file
  *		buffer to fit the patched file.
  *		Type: pointer
  *
  *	[file_buffer]
  *		See BP_file_buffer().
  *
  *	Optional, applied after a successful file replacement:
  *
  *	[file_buffer_addr_copy]
  *		Additional register to copy the file buffer to
  *		Type: pointer
  *
  *	[stack_clear_size]
  *		Value to add to ESP
  *		Type: hex
  *
  *	[eip_jump_dist]
  *		Value to add to EIP
  *		Type: hex
  *
  * Other breakpoints called
  * ------------------------
  *	BP_file_buffer
  */
THCRAP_BREAKPOINT_API int BP_file_load(x86_reg_t *regs, json_t *bp_info);

// These have been merged into BP_file_load() and are only kept for backwards
// compatibility.
TH_IMPORT int BP_file_name(x86_reg_t *regs, json_t *bp_info);
TH_IMPORT int BP_file_size(x86_reg_t *regs, json_t *bp_info);

/**
  * Placed at a position where a complete file is loaded into a single block
  * of memory, this breakpoint...
  * - applies a JSON patch on top of it if available
  * - dumps the original file if [dat_dump] in the run configuration is true
  *
  * Own JSON parameters
  * -------------------
  *	None
  *
  * Other breakpoints called
  * ------------------------
  *	BP_file_buffer
  */
THCRAP_BREAKPOINT_API int BP_file_loaded(x86_reg_t *regs, json_t *bp_info);

// Cool function name.
THCRAP_API int DumpDatFile(const char *dir, const char *name, const void *buffer, size_t size, bool overwrite_existing);

int bp_file_init(void);
void bp_file_mod_thread_exit(void);
int bp_file_exit(void);
