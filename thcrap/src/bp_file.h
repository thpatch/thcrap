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

	// For fragmented loading:
	// Original file size.
	size_t orig_size;

	// Offset of the file in the archive.
	size_t offset;
} file_rep_t;

// Initialize a file_rep_t object, and loads the replacement file and patch for file_name.
int file_rep_init(file_rep_t *fr, const char *file_name);

// Clears a file_rep_t object.
int file_rep_clear(file_rep_t *fr);

// Retrieves a file_rep_t object cached by BP_file_header
file_rep_t *file_rep_get(const char *filename);

// Retrieves a file_rep_t object cached by BP_file_header, using its object member
file_rep_t *file_rep_get_by_object(const void *object);
// Set the object member of a file_rep.
// If you want to use file_rep_get_by_object, you *must* set the object member with this function.
void file_rep_set_object(file_rep_t *fr, void *object);


/// Thread-local storage
/// --------------------
file_rep_t* fr_tls_get(void);
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
int BP_file_buffer(x86_reg_t *regs, json_t *bp_info);

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
int BP_file_load(x86_reg_t *regs, json_t *bp_info);

// These have been merged into BP_file_load() and are only kept for backwards
// compatibility.
int BP_file_name(x86_reg_t *regs, json_t *bp_info);
int BP_file_size(x86_reg_t *regs, json_t *bp_info);

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
  * Placed when the game parses its archive header. It will update
  * the file size and save the file informations.
  *
  * Own JSON parameters
  * -------------------
  *	[file_name]
  *		File name
  *		Type: immediate
  *
  *	[file_size]
  *		File size
  *		Type: pointer
  *
  * Other breakpoints called
  * ------------------------
  *	None
  */
int BP_file_header(x86_reg_t *regs, json_t *bp_info);

/**
  * Used when the a fragmented file reader loads its file.
  *
  * Own JSON parameters
  * -------------------
  *	[file_name]
  *		File name
  *		Type: immediate
  *
  *	[file_object]
  *		File object. Can be used to help fragmented_read_file to retrieve the file_rep
  *		Type: immediate
  *
  *	[file_size]
  *		File size. Optionnal if we didn't use BP_file_header, mandatory otherwise.
  *		Type: pointer
  *
  * Other breakpoints called
  * ------------------------
  *	None
  */
int BP_fragmented_open_file(x86_reg_t *regs, json_t *bp_info);

/**
  * Overwrites a ReadFile call and writes patched data into the buffer.
  * When apply is not true, this breakpoint just stores the filename for the next call.
  * When apply is true, this breakpoint must be exactly over a ReadFile call.
  * Parameters will be taken directly on the stack.
  *
  * Own JSON parameters
  * -------------------
  *	[file_name]
  *		File name
  *		Type: immediate
  *
  *	[file_object]
  *		File object, used instead of the file name if it isn't provided
  *		Type: immediate
  *
  *	[apply]
  *		Set it to true when you're on the ReadFile call
  *		Type: boolean
  *
  *	[post_read]
  *		Optional function called right after reading the original file (fragmented_read_file_hook_t).
  *		Type: immediate
  *
  *	[post_patch]
  *		Optional function called after the file is patched, or if is it doesn't need patching (fragmented_read_file_hook_t).
  *		Type: immediate
  *
  * Other breakpoints called
  * ------------------------
  *	None
  */
int BP_fragmented_read_file(x86_reg_t *regs, json_t *bp_info);
typedef void (*fragmented_read_file_hook_t)(const file_rep_t *fr, BYTE *buffer, size_t size);

/**
  * Used when the a fragmented file reader closes its file.
  *
  * Own JSON parameters
  * -------------------
  *	[file_object]
  *		File object, used instead of the last file used by the thread.
  *		Type: immediate
  *
  * Other breakpoints called
  * ------------------------
  *	None
  */
int BP_fragmented_close_file(x86_reg_t *regs, json_t *bp_info);

int bp_file_init(void);
void bp_file_mod_thread_exit(void);
int bp_file_exit(void);
