/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Repository handling.
  */

#pragma once

typedef struct {
	char *patch_id;
	char *title;
} repo_patch_t;

typedef struct {
	char *id;
	char *title;
	char *contact;
	char **servers;
	char **neighbors;
	repo_patch_t *patches;
} repo_t;

// Returns the local file name of a repository file.
TH_CALLER_FREE THCRAP_API char *RepoGetLocalFN(const char *id);

THCRAP_API repo_t *RepoLocalNext(HANDLE *hFind);

// Loads a repository from a repo.js.
THCRAP_API repo_t *RepoLoadJson(json_t *repo_js);

// Loads repository files from all subdirectories of the current directory.
THCRAP_API repo_t **RepoLoad(void);

// Write the repo.js for repo to repos/[repo_name]/repo.js
THCRAP_API bool RepoWrite(const repo_t *repo);

// Free a repo returned by RepoLoad, RepoLoadJson or RepoLocalNext.
THCRAP_API void RepoFree(repo_t *repo);
