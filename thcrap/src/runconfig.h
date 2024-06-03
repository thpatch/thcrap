/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Run configuration.
  */

#pragma once

// Print the entire runconfig to the log output
void runconfig_print();

// Returns the run configuration.
// Use this only for reading a value specific to a plugin.
const json_t *runconfig_json_get();

// Returns the thcrap directory
const char *runconfig_thcrap_dir_get();
// Returns the run configuration path
// If there are multiple ones, return only the 1st one
const char *runconfig_runcfg_fn_get();

// Returns the game id, for example "th06"
const char *runconfig_game_get();

// Returns the game build, for example "v1.00a"
const char *runconfig_build_get();

// Returns the command line parameters passed to the exe
const char *runconfig_cmdline_get();

#ifdef __cplusplus
extern "C++" {
	// Returns the thcrap directory
	std::string_view runconfig_thcrap_dir_get_view();

	// Returns the game id, for example "th06"
	std::string_view runconfig_game_get_view();

	// Returns the game build, for example "v1.00a"
	std::string_view runconfig_build_get_view();
}
#endif

// Returns the prettiest representation of a game title available,
// in this order:
// • 1. Localized game title from the string table
// • 2. "title" value from the run configuration
// • 3. The plain game ID
// • 4. NULL
const char *runconfig_title_get();

// Returns the game update url
const char *runconfig_update_url_get();

// Returns the dat dump path if dat dump is enabled,
// or NULL if it is disabled.
const char *runconfig_dat_dump_get();

// Returns the patched files dump path if patched files dump is enabled,
// or NULL if it is disabled.
const char *runconfig_patched_files_dump_get();

// Returns true if the current build in runconfig is a latest build, false otherwise
bool runconfig_latest_check();

// Returns the "true" latest build, the most recent one.
const char *runconfig_latest_get();

// Return the number of stages
size_t runconfig_stage_count();

// Return true if the binhack parser if it should show a message box, should it fail to find a function
bool runconfig_msgbox_invalid_func();

// Return the PID of the thcrap_loader process that started the exe
DWORD runconfig_loader_pid_get();

// Set the PID of the thcrap_loader process. For use in thcrap_loader
void runconfig_loader_pid_set(DWORD loader_pid);

#define RUNCFG_STAGE_DEFAULT 0
#define RUNCFG_STAGE_USE_MODULE	1
#define RUNCFG_STAGE_SKIP_BREAKPOINTS 2
// Apply the binhacks, codecaves and breakpoints of a stage.
// If flags contains RUNCFG_STAGE_USE_MODULE, module is used as the base
// for relative addresses Otherwise, the module parameter is ignored and
// the "module" value from the init stage data is used.
// If flags contains RUNCFG_STAGE_SKIP_BREAKPOINTS, the breakpoints
// won't be applied. Use that if you already applied them.
bool runconfig_stage_apply(size_t stage_num, int flags, HMODULE module);



/**
  * Internal functions
  */

typedef struct {
	uint8_t* address;
	size_t size;
} HackpointMemoryPage;

typedef struct {
	const char* name;
	size_t offset;
	size_t stage_num;
} HackpointMemoryName;

HackpointMemoryName locate_address_in_stage_pages(void* FindAddress);

#define RUNCONFIG_NO_OVERWRITE 1
#define RUNCONFIG_NO_BINHACKS 2

// Load a run configuration.
// If called several times, the new configuration will be merged with the old one
// (the new values overwrite the older ones unless RUNCONFIG_NO_OVERWRITE is specified).
void runconfig_load(json_t *file, int flags);

// Load a run configuration from a file.
void runconfig_load_from_file(const char *path);

// Free the run configuration.
// You can load a new run configuration after calling this function.
void runconfig_free();


// Set the thcrap directory
void runconfig_thcrap_dir_set(const char *thcrap_dir);

// Set the game build
void runconfig_build_set(const char *build);
