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
char *RepoGetLocalFN(const char *id);

repo_t *RepoLocalNext(HANDLE *hFind);

// Loads a repository from a repo.js.
repo_t *RepoLoadJson(json_t *repo_js);

// Loads repository files from all subdirectories of the current directory.
repo_t **RepoLoad(void);

// Free a repo returned by RepoLoad, RepoLoadJson or RepoLocalNext.
void RepoFree(repo_t *repo);
