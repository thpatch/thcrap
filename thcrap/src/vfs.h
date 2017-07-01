/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Virtual file system.
  */

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

typedef json_t* jsonvfs_generator_t(std::unordered_map<std::string, json_t*> in_data, const std::string out_fn, size_t* out_size);

extern "C" {
	/**
	  * Add a handler to create files in the virtual file system.
	  *
	  * in_fns contains a list of json files needed to generate the virtual files.
	  *
	  * Each time a jdiff file matching out_pattern is resolved, gen will be called.
	  * If it returns a non-NULL value, the return value will be used as if a file
	  * with this content exists at the top of the patch stack.
	  */
	void jsonvfs_add(const std::string out_pattern, std::unordered_set<std::string> in_fns, jsonvfs_generator_t *gen);

	// Same as jsonvfs_add, but all file names are game-relative.
	void jsonvfs_game_add(const std::string out_pattern, std::unordered_set<std::string> in_fns, jsonvfs_generator_t *gen);

	// Return a file from the vfs if it exists.
	json_t *jsonvfs_get(const std::string fn, size_t* size);
}