/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * DLL and engine initialization.
  */

#pragma once

typedef struct game_version
{
	char *id;
	char *build;
	char *variety;
	unsigned int codepage;

#ifdef __cplusplus
	game_version(json_t *json)
		: id(     strdup(json_array_get_string(json, 0))),
		  build(  strdup(json_array_get_string(json, 1))),
		  variety(strdup(json_array_get_string(json, 2))),
		  codepage((unsigned int)json_array_get_hex(json, 3))
	{}
#endif
} game_version;

TH_CALLER_CLEANUP(identify_free) THCRAP_API game_version* identify_by_hash(const char *fn, size_t *exe_size, json_t *versions);
TH_CALLER_CLEANUP(identify_free) THCRAP_API game_version* identify_by_size(size_t exe_size, json_t *versions);

// Identifies the game, version and variety of [fn] by looking up its hash
// and file size in versions.js.
// Also shows a message box in case an unknown version was detected.
// Returns a fully merged run configuration on successful identification,
// NULL on failure or user cancellation.
THCRAP_API json_t* identify(const char *fn);

// Free the result of an identify function
THCRAP_API void identify_free(game_version *ver);

// Applies the detour cache to the module at [hProc].
THCRAP_API void thcrap_detour(HMODULE hProc);

// Sets up the engine with the given configuration and the correct game-
// specific files for the current process.
THCRAP_API int thcrap_init(const char *setup_fn);

// Second part of thcrap_init(), applies any sort of binary change to the
// current process, using the binary hacks and breakpoints from the stage
// with the given number.
// If module is not NULL, it is used as the base for relative addresses.
// If it is NULL (which is the case for all calls that didn't come from
// BP_init_next_stage), it is ignored and the "module" value from
// the init stage data is used.
THCRAP_INTERNAL_API int thcrap_init_binary(size_t stage_num, HMODULE module);

/**
  * Sets up the binary hacks and breakpoints of the next stage.
  *
  * Own JSON parameters
  * -------------------
  *	[module] (optional)
  *		Custom base address for all relative addresses in binary hacks or
  *		breakpoints of the next stage. Defaults to a nullptr, representing
  *		the main module of the current process. This always overrides any
  *		"module" value that is part of the init stage data.
  *		Type: immediate
  *
  * Other breakpoints called
  * ------------------------
  *	None
  */
THCRAP_BREAKPOINT_API int BP_init_next_stage(x86_reg_t *regs, json_t *bp_info);

// If the target process terminates using ExitProcess(), any active threads
// will have most likely already been terminated before DLL_PROCESS_DETACH is
// sent to DllMain().
// ("Most likely" because yes, the implementation is subject to change, see
// http://blogs.msdn.com/b/oldnewthing/archive/2007/05/03/2383346.aspx.)
// As a result, we have to detour this function to properly shut down any
// threads created by thcrap plugins or modules before calling ExitProcess().
TH_NORETURN void WINAPI thcrap_ExitProcess(UINT uExitCode);
