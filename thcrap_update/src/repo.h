/**
  * Touhou Community Reliant Automatic Patcher
  * Update Plugin
  *
  * ----
  *
  * Repository handling.
  */

#pragma once

// Starts repository discovery at [start_url], recurses through all neighbors
// and downloads any repository files found in the process.
// [url_cache] and [id_cache] are used to detect duplicate URLs and repos.
// These can be NULL if the caller doesn't intend to keep these caches.
int RepoDiscoverAtURL(const char *start_url, json_t *id_cache, json_t *url_cache);

// Calls RepoDiscover() for the repositories in all subdirectories of the
// current directory, as well as their neighbors.
int RepoDiscoverFromLocal(json_t *id_cache, json_t *url_cache);
