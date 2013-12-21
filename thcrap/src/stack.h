/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Patch stack evaluators and information.
  */

#pragma once

/// File resolution
/// ---------------
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
/// ---------------

/// Information
/// -----------
// Displays a message box showing missing archives in the current patch stack,
// if there are any.
void stack_show_missing(void);
/// -----------
