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
const DWORD PROJECT_VERSION(void);
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

// Returns a pointer to a function named [func] in the list of exported functions.
// Basically a GetProcAddress across the engine and all loaded plug-ins.
void* runconfig_func_get(const char *name);
