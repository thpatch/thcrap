/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Globals, compile-time constants and runconfig abstractions.
  */

#pragma once

#define ENABLE_OVERHAULED_UPDATE_SETTINGS 0

enum {
	UPDATE_AT_STARTUP = 0,
	UPDATE_IN_BACKGROUND = 1,
	UPDATE_AT_EXIT = 2
};
typedef uint8_t update_type_t;

enum {
	LOADER_HIDDEN = 0,
	LOADER_STARTUP_ONLY = 1,
	LOADER_PERSISTENT = 2
};
typedef uint8_t loader_type_t;

#define DEFAULT_UPDATE_TYPE UPDATE_AT_STARTUP
#define DEFAULT_LOADER_TYPE LOADER_STARTUP_ONLY

// Project stats
THCRAP_API extern const char PROJECT_NAME[];
THCRAP_API extern const char PROJECT_NAME_SHORT[];
THCRAP_API extern const char PROJECT_URL[];
THCRAP_API extern const uint32_t PROJECT_VERSION;
THCRAP_API extern const char PROJECT_VERSION_STRING[];
THCRAP_API extern const char PROJECT_BRANCH[];

// Returns the value matching key in config converted in bool
THCRAP_API BOOL globalconfig_get_boolean(const char* key, const BOOL default_value);
// Sets the value in config and then writes the result on disk
// It returns what json_dump_file returns
THCRAP_API int globalconfig_set_boolean(const char* key, const BOOL value);
// Returns the value matching key in config converted in long long
THCRAP_API long long globalconfig_get_integer(const char* key, const long long default_value);
// Sets the value in config and then writes the result on disk
// It returns what json_dump_file returns
THCRAP_API int globalconfig_set_integer(const char* key, const long long value);
// Returns the value matching key in config as a string
THCRAP_API const char* globalconfig_get_string(const char* key, const char* default_value);
// Sets the value in config and then writes the result on disk
// It returns what json_dump_file returns
THCRAP_API int globalconfig_set_string(const char* key, const char* value);
// Releases global_cfg
THCRAP_INTERNAL_API void globalconfig_release(void);

// Memory management
THCRAP_API void* TH_CDECL thcrap_alloc(size_t size);
THCRAP_API void  TH_CDECL thcrap_free(void *mem);

#if TH_X86
#define VERSIONS_SUFFIX
#define VERSIONS_SUFFIX_ALT "64"
#define CURRENT_ARCH_BITS 32
#define ALT_ARCH_BITS 64
#else
#define VERSIONS_SUFFIX "64"
#define VERSIONS_SUFFIX_ALT
#define CURRENT_ARCH_BITS 64
#define ALT_ARCH_BITS 32
#endif

// Convenience macros for binary file names that differ between builds.
#if NDEBUG
#define DEBUG_OR_RELEASE
#define DEBUG_OR_RELEASE_W
#if TH_X86
#define FILE_SUFFIX
#define FILE_SUFFIX_W
#define ALT_FILE_SUFFIX "_64"
#define ALT_FILE_SUFFIX_W L"_64"
#else
#define FILE_SUFFIX "_64"
#define FILE_SUFFIX_W L"_64"
#define ALT_FILE_SUFFIX
#define ALT_FILE_SUFFIX_W
#endif
#else
#define DEBUG_OR_RELEASE "_d"
#define DEBUG_OR_RELEASE_W L"_d"
#if TH_X86
#define FILE_SUFFIX "_d"
#define FILE_SUFFIX_W L"_d"
#define ALT_FILE_SUFFIX "_64_d"
#define ALT_FILE_SUFFIX_W L"_64_d"
#else
#define FILE_SUFFIX "_64_d"
#define FILE_SUFFIX_W L"_64_d"
#define ALT_FILE_SUFFIX "_d"
#define ALT_FILE_SUFFIX_W L"_d"
#endif
#endif
