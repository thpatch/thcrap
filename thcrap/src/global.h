/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Globals, compile-time constants and runconfig abstractions.
  */

#pragma once

// Project stats
const char* PROJECT_NAME(void);
const char* PROJECT_NAME_SHORT(void);
const char* PROJECT_URL(void);
DWORD PROJECT_VERSION(void);
const char* PROJECT_VERSION_STRING(void);
const char* PROJECT_BRANCH(void);

// Returns the value matching key in config converted in bool
// If key isn't in config it returns default_value with errno ENOENT
BOOL globalconfig_get_boolean(char* key, const BOOL default_value);
// Sets the value in config and then writes the result on disk
// It returns what json_dump_file returns
int globalconfig_set_boolean(char* key, const BOOL value);
// Returns the value matching key in config converted in long long
// If key isn't in config it returns default_value with errno ENOENT
long long globalconfig_get_integer(char* key, const long long default_value);
// Sets the value in config and then writes the result on disk
// It returns what json_dump_file returns
int globalconfig_set_integer(char* key, const long long value);
// Releases global_cfg
void globalconfig_release(void);

// Memory management
void* __cdecl thcrap_alloc(size_t size);
void  __cdecl thcrap_free(void *mem);

// Convenience macro for binary file names that differ between Debug and
// Release builds.
#ifdef _DEBUG
# define DEBUG_OR_RELEASE "_d"
#else
# define DEBUG_OR_RELEASE
#endif
