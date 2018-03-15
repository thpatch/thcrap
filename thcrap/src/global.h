/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Globals, compile-time constants and runconfig abstractions.
  */

#pragma once

// We only want to read from or write to one file at a time
extern CRITICAL_SECTION cs_file_access;

extern json_t *run_cfg;

// Project stats
const char* PROJECT_NAME(void);
const char* PROJECT_NAME_SHORT(void);
DWORD PROJECT_VERSION(void);
const char* PROJECT_VERSION_STRING(void);

json_t* runconfig_get(void);
void runconfig_set(json_t *new_run_cfg);

// Returns the prettiest representation of a game title available,
// in this order:
// • 1. Localized game title from the string table
// • 2. "title" value from the run configuration
// • 3. The plain game ID
// • 4. NULL
const json_t *runconfig_title_get(void);

// Convenience macro for binary file names that differ between Debug and
// Release builds.
#ifdef _DEBUG
# define DEBUG_OR_RELEASE "_d"
#else
# define DEBUG_OR_RELEASE
#endif
