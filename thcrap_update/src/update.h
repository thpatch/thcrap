/**
  * Touhou Community Reliant Automatic Patcher
  * Update plugin
  *
  * ----
  *
  * Main updating functionality.
  */

#pragma once

typedef enum {
	GET_OUT_OF_MEMORY = -2,
	GET_INVALID_PARAMETER = -1,
	GET_OK = 0,
	GET_SERVER_ERROR,
	GET_NOT_FOUND,
} get_result_t;

int http_init(void);
void http_exit(void);

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

// High-level patch and stack updates.
// These can optionally take a filter function to limit the number of files
// downloaded.
typedef int (*update_filter_func_t)(const char *fn, json_t *filter_data);

// Returns 1 for all global file names, i.e. those without a slash.
int update_filter_global(const char *fn, json_t *null);
// Returns 1 for all global file names and those that are specific to a game
// in the flexible JSON array [games].
int update_filter_games(const char *fn, json_t *games);

int patch_update(
	json_t *patch_info, update_filter_func_t filter_func, json_t *filter_data
);
void stack_update(update_filter_func_t filter_func, json_t *filter_data);
