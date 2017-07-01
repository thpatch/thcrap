/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Repository handling.
  */

#pragma once

// Returns the local file name of a repository file as a JSON string.
json_t* RepoGetLocalFN(const char *id);

json_t* RepoLocalNext(HANDLE *hFind);

// Loads repository files from all subdirectories of the current directory.
// Returns a JSON object in the following format:
// {
//	"<repository ID>": {
//		<contents of repo.js>
//	},
//	...
// }
json_t* RepoLoad(void);
