/**
  * Touhou Community Reliant Automatic Patcher
  * Update plugin
  *
  * ----
  *
  * Main updating functionality.
  */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	GET_CANCELLED = -3,
	GET_OUT_OF_MEMORY = -2,
	GET_INVALID_PARAMETER = -1,
	GET_OK = 0,
	GET_SERVER_ERROR,
	GET_NOT_AVAILABLE, // 404, 502, etc.
} get_result_t;

// These callbacks should return TRUE if the download is allowed to continue and FALSE to cancel it.
typedef int (*file_callback_t)(const char *fn, get_result_t ret, DWORD file_progress, DWORD file_size, void *param);
typedef int (*patch_update_callback_t)(const json_t *patch, DWORD patch_progress, DWORD patch_total, const char *fn, get_result_t ret, DWORD file_progress, DWORD file_total, void *param);
typedef int (*stack_update_callback_t)(DWORD stack_progress, DWORD stack_total, const json_t *patch, DWORD patch_progress, DWORD patch_total, const char *fn, get_result_t ret, DWORD file_progress, DWORD file_total, void *param);

int http_init(void);
void http_exit(void);

// Tries to download the given [fn] from any server in [servers].
// If successful, [file_size] receives the size of the downloaded file.
// [exp_crc] can be optionally given to enforce the downloaded file to have a
// certain checksum. If it doesn't match for one server, another one is tried,
// until none are left. To disable this check, simply pass NULL.
void* ServerDownloadFile(
	json_t *servers, const char *fn, DWORD *file_size, const DWORD *exp_crc, file_callback_t callback, void *callback_param
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

// Bootstraps the patch selection [sel] by building a patch object, downloading
// patch.js from [repo_servers], and storing it inside the returned object.
json_t* patch_bootstrap(const json_t *sel, json_t *repo_servers);

int patch_update(
	json_t *patch_info, update_filter_func_t filter_func, json_t *filter_data, patch_update_callback_t callback, void *callback_param
);
void stack_update(update_filter_func_t filter_func, json_t *filter_data, stack_update_callback_t callback, void *callback_param);

#ifdef __cplusplus
}

/// Internal C++ server connection handling
/// ---------------------------------------
#include <vector>

struct server_t {
	// This either comes from a JSON object with a lifetime longer than any
	// instance of this structure, or a hardcoded string, so no need to use
	// std::string here.
	// TODO: Turn this into a std::string_view once we've migrated to a C++17
	// compiler.
	const char *url = NULL;

	// Last 5 ping times of this server.
	// The raw counter value is enough for our purposes, no need to lose
	// precision by dividing through the frequency.
	LONGLONG ping[5];

	LONGLONG ping_average() const;
	void ping_push(LONGLONG newval);

	bool  active() const { return this->ping_average() >= 0; }
	bool  unused() const { return this->ping_average() == 0; }
	bool visited() const { return this->ping_average()  > 0; }

	void disable() {
		for(auto& i : this->ping) {
			i = -1;
		}
	}

	void new_session() {
		for(auto& i : this->ping) {
			i = 0;
		}
	}

	// Single-server part of servers_t::download().
	void* download(
		DWORD *file_size, get_result_t *ret, const char *fn, const DWORD *exp_crc, file_callback_t callback = nullptr, void *callback_param = nullptr
	);

	server_t() {
	}

	server_t(const char *_url) : url(_url) {
		// TODO: For consistency, ping should be initialized with {0},
		// but Visual Studio 2013 doesn't implement explicit
		// initializers for arrays.
		new_session();
 	}
};

struct servers_t : std::vector<server_t> {
	// Returns the index of the first server to try. This selects the fastest
	// server based on the measurements of previous download times.
	int get_first() const;

	// Returns the number of active servers.
	int num_active() const;

	// Internal version of ServerDownloadFile().
	void* download(DWORD *file_size, const char *fn, const DWORD *exp_crc, file_callback_t callback = nullptr, void *callback_param = nullptr);

	// Fills this instance with data from a JSON "servers" array as used
	// in repo.js and patch.js.
	void from(const json_t *servers);

	servers_t() {
	}

	servers_t(const char *url) : std::vector<server_t>({url}) {
	}
};
/// ---------------------------------------
#endif
