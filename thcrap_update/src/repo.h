/**
  * Touhou Community Reliant Automatic Patcher
  * Update module
  *
  * ----
  *
  * Repository handling.
  */

#pragma once

// Called whenever a write to [fn] failed. Should show a helpful message and
// return whether to continue (1) or abort (0).
typedef int file_write_error_t(const char *fn);

// Starts repository discovery at [start_url], recurses through all neighbors
// and downloads any repository files found in the process.
// [url_cache] and [id_cache] are used to detect duplicate URLs and repos.
// These can be NULL if the caller doesn't intend to keep these caches.
// Returns 1 if this process was prematurely aborted due to a file write
// error. If [fwe_callback] is NULL, this will never happen.
int RepoDiscoverAtURL(const char *start_url, json_t *id_cache, json_t *url_cache, file_write_error_t *fwe_callback);

// Calls RepoDiscover() for the repositories in all subdirectories of the
// current directory, as well as their neighbors.
// Returns 1 if this process was prematurely aborted due to a file write
// error. If [fwe_callback] is NULL, this will never happen.
int RepoDiscoverFromLocal(json_t *id_cache, json_t *url_cache, file_write_error_t *fwe_callback);
