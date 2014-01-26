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

int thcrap_init(const char *setup_fn);
