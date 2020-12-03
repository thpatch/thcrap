/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Repository handling.
  */

#pragma once

enum PatchFlags
{
	PATCH_FLAG_CORE       = 0x01,
	PATCH_FLAG_LANGUAGE   = 0x02,
	PATCH_FLAG_HIDDEN     = 0x04,
	PATCH_FLAG_GAMEPLAY   = 0x08,
	PATCH_FLAG_GRAPHICS   = 0x10,
	PATCH_FLAG_FANFICTION = 0x20,
	PATCH_FLAG_BGM        = 0x40,
};

typedef struct {
	char *patch_id;
	char *title;
	unsigned int flags;
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

// Write the repo.js for repo to repos/[repo_name]/repo.js
bool RepoWrite(const repo_t *repo);

// Free a repo returned by RepoLoad, RepoLoadJson or RepoLocalNext.
void RepoFree(repo_t *repo);
