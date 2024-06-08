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
THCRAP_API extern const char PROJECT_NAME[];
THCRAP_API extern const char PROJECT_NAME_SHORT[];
THCRAP_API extern const char PROJECT_URL[];
THCRAP_API extern const uint32_t PROJECT_VERSION;
THCRAP_API extern const char PROJECT_VERSION_STRING[];
THCRAP_API extern const char PROJECT_BRANCH[];

// Initializes global_cfg
THCRAP_INTERNAL_API void globalconfig_init(void);
// Returns the value matching key in config converted in bool
// If key isn't in config it returns default_value with errno ENOENT
THCRAP_API BOOL globalconfig_get_boolean(const char* key, const BOOL default_value);
// Sets the value in config and then writes the result on disk
// It returns what json_dump_file returns
THCRAP_API int globalconfig_set_boolean(const char* key, const BOOL value);
// Returns the value matching key in config converted in long long
// If key isn't in config it returns default_value with errno ENOENT
THCRAP_API long long globalconfig_get_integer(const char* key, const long long default_value);
// Sets the value in config and then writes the result on disk
// It returns what json_dump_file returns
THCRAP_API int globalconfig_set_integer(const char* key, const long long value);
// Returns the value matching key in config as a string
// If key isn't in config it returns default_value with errno ENOENT
THCRAP_API const char* globalconfig_get_string(const char* key, const char* default_value);
// Sets the value in config and then writes the result on disk
// It returns what json_dump_file returns
THCRAP_API int globalconfig_set_string(const char* key, const char* value);
// Releases global_cfg
THCRAP_INTERNAL_API void globalconfig_release(void);

// Memory management
THCRAP_API void* TH_CDECL thcrap_alloc(size_t size);
THCRAP_API void  TH_CDECL thcrap_free(void *mem);

// Convenience macro for binary file names that differ between Debug and
// Release builds.
#ifdef _DEBUG
# define DEBUG_OR_RELEASE "_d"
# define DEBUG_OR_RELEASE_W L"_d"
#else
# define DEBUG_OR_RELEASE
# define DEBUG_OR_RELEASE_W
#endif

#if TH_X86
#define VERSIONS_SUFFIX
#define VERSIONS_SUFFIX_ALT "64"
#else
#define VERSIONS_SUFFIX "64"
#define VERSIONS_SUFFIX_ALT
#endif

#if NDEBUG
#if TH_X86
#define FILE_SUFFIX
#define FILE_SUFFIX_W
#else
#define FILE_SUFFIX "_64"
#define FILE_SUFFIX_W L"_64"
#endif
#else
#if TH_X86
#define FILE_SUFFIX "_d"
#define FILE_SUFFIX_W L"_d"
#else
#define FILE_SUFFIX "_64_d"
#define FILE_SUFFIX_W L"_64_d"
#endif
#endif
