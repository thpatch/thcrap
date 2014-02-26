/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * DLL and engine initialization.
  */

#pragma once

int IsLatestBuild(json_t *build_obj, json_t **latest, json_t *run_ver);

json_t* identify_by_hash(const char *fn, size_t *exe_size, json_t *versions);
json_t* identify_by_size(size_t exe_size, json_t *versions);

// Identifies the game, version and variety of [fn],
// using hash and size lookup in versions.js.
// Also shows message boxes in case an unknown or outdated version was detected.
// Returns a fully merged run configuration on successful identification,
// NULL on failure or user cancellation.
json_t* identify(const char *fn);

// All Import Address Table detour calls required by the engine.
void thcrap_detour(HMODULE hProc);

// Sets up the engine with the given configuration for the current process.
int thcrap_init(const char *setup_fn);

// If the target process terminates using ExitProcess(), any active threads
// will have most likely already been terminated before DLL_PROCESS_DETACH is
// sent to DllMain().
// ("Most likely" because yes, the implementation is subject to change, see
// http://blogs.msdn.com/b/oldnewthing/archive/2007/05/03/2383346.aspx.)
// As a result, we have to detour this function to properly shut down any
// threads created by thcrap plugins or modules before calling ExitProcess().
DECLSPEC_NORETURN VOID WINAPI thcrap_ExitProcess(__in UINT uExitCode);
