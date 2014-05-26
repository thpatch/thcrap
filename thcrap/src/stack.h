/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Patch stack evaluators and information.
  */

#pragma once

// Iteration direction
typedef enum {
	SCI_BACKWARDS = -1,
	SCI_FORWARDS = 1
} sci_dir_t;

// Iteration state. [patch_info] and [fn] hold the current patch and chain
// file name after each call to stack_chain_iterate().
typedef struct {
	const json_t *patches;
	int step;
	json_t *patch_info;
	const char *fn;
} stack_chain_iterate_t;

/// File resolution
/// ---------------
// Creates a JSON array containing the the default resolving chain for [fn].
// The chain currently consists of [fn] itself, followed by its build-specific
// name returned by fn_for_build().
// All resolving functions that take a file name (instead of a chain) as a
// parameter should use the chain created by this function.
json_t* resolve_chain(const char *fn);

// Builds a chain for a game-local file name.
json_t* resolve_chain_game(const char *fn);

// Repeatedly iterate through the stack using the given resolving [chain].
// [sci] keeps the iteration state.
int stack_chain_iterate(stack_chain_iterate_t *sci, const json_t *chain, sci_dir_t direction);

// Walks through the patch stack configured in <run_cfg>, merging every file
// with the filename [fn] into a single JSON object. Returns the merged JSON
// object or NULL if there is no matching file in the patch stack.
// If given, [file_size] receives a _rough estimate_ of the JSON file size.
json_t* stack_json_resolve_chain(const json_t *chain, size_t *file_size);
json_t* stack_json_resolve(const char *fn, size_t *file_size);

// Generic file resolver. Returns the file matching the [chain] with the
// highest priority inside the patch stack.
void* stack_file_resolve_chain(const json_t *chain, size_t *file_size);

// Search the patch stack configured in <run_cfg> for a replacement for the game data file [fn].
// Returns the loaded patch file or NULL if the file is not to be patched.
void* stack_game_file_resolve(const char *fn, size_t *file_size);

// Resolves a game-local JSON file.
json_t* stack_game_json_resolve(const char *fn, size_t *file_size);
/// ---------------

/// Information
/// -----------
// Displays a message box showing missing archives in the current patch stack,
// if there are any.
void stack_show_missing(void);
/// -----------
