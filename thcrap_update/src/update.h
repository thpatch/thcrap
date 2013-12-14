/**
  * Touhou Community Reliant Automatic Patcher
  * Update plugin
  *
  * ----
  *
  * Main updating functionality.
  */

#pragma once

int inet_init(void);
void inet_exit(void);

// Builds a server object from [start_url].
json_t* ServerBuild(const char *start_url);

// Initializes the <servers> object in [patch_js].
json_t* ServerInit(json_t *patch_js);

// "Initiates a new downloading session".
// That is, resets all time values of the [servers].
void ServerNewSession(json_t *servers);

// Returns the index of the first server to try.
// This selects the fastest server based on the measurements of previous download times.
const int ServerGetFirst(const json_t *servers);

// Returns the number of active [servers].
size_t ServerGetNumActive(const json_t *servers);

int ServerDisable(json_t *server);

// Tries to download the given [fn] from any server in [servers].
// If successful, [file_size] receives the size of the downloaded file.
// [exp_crc] can be optionally given to enforce the downloaded file to have a
// certain checksum. If it doesn't match for one server, another one is tried,
// until none are left. To disable this check, simply pass NULL.
void* ServerDownloadFile(
	json_t *servers, const char *fn, DWORD *file_size, const DWORD *exp_crc
);

// Updates the patch in [patch_info].
int patch_update(const json_t *patch_info);
