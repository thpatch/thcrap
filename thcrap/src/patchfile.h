/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Read and write access to the files of a patch.
  */

#pragma once

/**
  * Patch function type.
  *
  * Parameters
  * ----------
  *	BYTE* file_inout
  *		Buffer containing the original file.
  *		Should contain the patched result after this function.
  *
  *	size_t size_out
  *		Size of [file_inout]. Can be larger than [size_in].
  *
  *	size_t size_in
  *		Size of the original file.
  *
  *	json_t *patch
  *		Patch data to be applied.
  *
  *	json_t *run_cfg
  *		The current run configuration.
  *
  * Returns nothing.
  */
typedef int (*func_patch_t)(BYTE* file_inout, size_t size_out, size_t size_in, json_t *patch, json_t *run_cfg);

// Reads the file [fn] into a newly created buffer and returns its file size in [file_size].
// Return value has to be free()d by the caller!
void* file_read(const char *fn, size_t *file_size);

/// ----------
/// File names
/// ----------
// Returns the full patch-relative name of a game-relative file.
// Return value has to be free()d by the caller!
char* fn_for_game(const char *fn);
/// ----------

/// ------------------------
/// Single-patch file access
/// ------------------------
// Returns 1 if the file [fn] exists in [patch_info].
int patch_file_exists(const json_t *patch_info, const char *fn);

// Returns 1 is the file name [fn] is blacklisted by [patch_info].
int patch_file_blacklisted(const json_t *patch_info, const char *fn);

// Loads the file [fn] from [patch_info].
void* patch_file_load(const json_t *patch_info, const char *fn, size_t *file_size);

// Loads the JSON file [fn] from [patch_info].
// If given, [file_size] receives the size of the input file.
json_t* patch_json_load(const json_t *patch_info, const char *fn, size_t *file_size);

int patch_file_store(const json_t *patch_info, const char *fn, const void *file_buffer, const size_t file_size);
int patch_json_store(const json_t *patch_info, const char *fn, const json_t *json);
/// ------------------------

/// ----------------------
/// Patch stack evaluators
/// ----------------------
// Walks through the patch stack configured in <run_cfg>,
// merging every file with the filename [fn] into a single JSON object.
// Returns the merged JSON object or NULL if there is no matching file in the patch stack.
// If given, [file_size] receives a _rough estimate_ of the JSON file size.
json_t* stack_json_resolve(const char *fn, size_t *file_size);

// Search the patch stack configured in <run_cfg> for a replacement for the game data file [fn].
// Returns the loaded patch file or NULL if the file is not to be patched.
void* stack_game_file_resolve(const char *fn, size_t *file_size);

// Resolves a game-local JSON file.
json_t* stack_game_json_resolve(const char *fn, size_t *file_size);
/// ----------------------

/// -----
/// Hooks
/// -----
// Register a hook function for a certain file extension.
int patchhook_register(const char *ext, func_patch_t patch_func);

// Builds an array of patch hook functions for [fn].
// Has to be json_decref()'d by the caller!
json_t* patchhooks_build(const char *fn);

// Runs all hook functions in [hook_array] on the given data.
int patchhooks_run(const json_t *hook_array, void *file_inout, size_t size_out, size_t size_in, json_t *patch, json_t *run_cfg);
/// -----
