/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Read and write access to the files of a single patch.
  */

#pragma once

/**
  * Patch function type.
  *
  * Parameters
  * ----------
  *	void* file_inout
  *		Buffer containing the original file.
  *		Should contain the patched result after this function.
  *
  *	size_t size_out
  *		Size of [file_inout]. Can be larger than [size_in].
  *
  *	size_t size_in
  *		Size of the original file.
  *
  *	const char *fn
  *		Filename of the file.
  *
  *	json_t *patch
  *		Patch data to be applied.
  *
  * Returns a value >0 if the hook changed the file in file_inout, and a value <=0 otherwise.
  */
typedef int (*func_patch_t)(void* file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch);

/**
  * Patch file guessing function type.
  *
  * Parameters
  * ----------
  *	const char *fn
  *		Filename of the file.
  *
  *	json_t *patch
  *		Patch data to be applied.
  *
  *	size_t patch_size
  *		Size of the patfh file.
  *
  * If the patch function may need to increase the file size, return the number of bytes it will need.
  * Else, return 0.
  */
typedef size_t (*func_patch_size_t)(const char *fn, json_t *patch, size_t patch_size);

// Short description of a patch.
// Used for patch selection in thcrap_configure, and for dependencies.
typedef struct
{
	char *repo_id;
	char *patch_id;
} patch_desc_t;

// Patch data from runconfig and patch.js
typedef struct
{
	// Patch root path
	// Pulled from the run configuration then edited to make it an absolute path
	char *archive;
	// Patch id (from patch.js)
	char *id;
	// Patch version (from patch.js)
	uint32_t version;
	// Patch description (from patch.js)
	char *title;
	// Servers list (NULL-terminated) (from patch.js)
	char **servers;
	// List of dependencies (from patch.js)
	patch_desc_t *dependencies;
	// List of font files to load (from patch.js)
	char **fonts;
	// List of files to ignore (NULL-terminated) (from run configuration)
	char **ignore;
	// If false, the updater should ignore this patch
	bool update;
	// Patch index in the stack, used for pretty-printing
	size_t level;
	// User set patch configuration
	json_t *config;

	// MOTD: message that can be displayed by a patch on startup (from patch.js)
	// Message content
	char *motd;
	// Message title (optional)
	char *motd_title;
	// Message type. See the uType parameter in https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-messagebox
	// Optional, defaults to 0 (MB_OK)
	DWORD motd_type;
} patch_t;

// Enum of possible types for the description of
// a value specified by the user defined patch options
enum patch_opt_val_type {
	PATCH_OPT_VAL_INVALID = -1,
	PATCH_OPT_VAL_BYTE = 0,
	PATCH_OPT_VAL_SBYTE = 1,
	PATCH_OPT_VAL_WORD = 2,
	PATCH_OPT_VAL_SWORD = 3,
	PATCH_OPT_VAL_DWORD = 4,
	PATCH_OPT_VAL_SDWORD = 5,
	PATCH_OPT_VAL_FLOAT = 6,
	PATCH_OPT_VAL_DOUBLE = 7,
};

// Description of a value specified by the options
typedef struct {
	uint32_t t;
	uint32_t size;
	union type_t {
		BYTE byte;
		signed char sbyte;
		WORD word;
		signed short sword;
		DWORD dword;
		signed long sdword;
		float f;
		double d;
		BYTE byte_array[8];
	} val;
} patch_opt_val_t;

// Parses and error checks patch options from game_id.js
void patch_opts_from_json(json_t *opts);

// Obtains the value of a patch option
patch_opt_val_t* patch_opt_get(const char *name);

// Opens the file [fn] for read operations. Just a lightweight wrapper around
// CreateFile(): Returns INVALID_HANDLE_VALUE on failure, and the caller must
// call CloseHandle() on the returned value.
HANDLE file_stream(const char *fn);

// Reads the given file [stream] into a newly created buffer and optionally
// returns its [file_size]. If given, [file_size] is guaranteed to be set
// to 0 on failure. The returned buffer has to be free()d by the caller!
void* file_stream_read(HANDLE stream, size_t *file_size);

// Combines file_stream() and file_stream_read().
void* file_read(const char *fn, size_t *file_size);

// Writes [file_buffer] to a file named [fn]. The file is always overwritten!
// Returns 0 on success, or a Win32 error code on failure.
int file_write(const char *fn, const void *file_buffer, const size_t file_size);

/// ----------
/// File names
/// ----------
// Returns the full patch-relative name of a game-relative file.
// Return value has to be free()d by the caller!
char* fn_for_game(const char *fn);

// Returns the alternate file name for [fn] specific to the
// currently running build. Return value has to be free()d by the caller!
char* fn_for_build(const char *fn);

// Returns the full path of a patch-relative file name.
// Return value has to be free()d by the caller!
char* fn_for_patch(const patch_t *patch_info, const char *fn);

// Prints the full path of a patch-relative file name to the log.
void patch_print_fn(const patch_t *patch_info, const char *fn);
/// ----------

/// -----------
/// Directories
/// -----------
// Recursively creates directories until [fn] can be stored. (`mkdir -p`)
int dir_create_for_fn(const char *fn);
/// -----------

/// ------------------------
/// Single-patch file access
/// ------------------------
// Returns 1 if the file [fn] exists in [patch_info].
int patch_file_exists(const patch_t *patch_info, const char *fn);

// Returns 1 if the file name [fn] is blacklisted by [patch_info].
int patch_file_blacklisted(const patch_t *patch_info, const char *fn);

// Loads the file [fn] from [patch_info].
// Used analogous to file_stream() and file_stream_read().
HANDLE patch_file_stream(const patch_t *patch_info, const char *fn);
void* patch_file_load(const patch_t *patch_info, const char *fn, size_t *file_size);

// Loads the JSON file [fn] from [patch_info].
// If given, [file_size] receives the size of the input file.
json_t* patch_json_load(const patch_t *patch_info, const char *fn, size_t *file_size);

// Loads [fn] from the patch [patch_info] and merges it into [json_inout].
// Returns the file size of [fn].
size_t patch_json_merge(json_t **json_inout, const patch_t *patch_info, const char *fn);

// These return the result of file_write().
int patch_file_store(const patch_t *patch_info, const char *fn, const void *file_buffer, const size_t file_size);
int patch_json_store(const patch_t *patch_info, const char *fn, const json_t *json);

int patch_file_delete(const patch_t *patch_info, const char *fn);
/// ------------------------

/// Information
/// -----------
// Shows the MOTD of the patch.
void patch_show_motd(const patch_t *patch_info);
/// -----------

/// Initialization
/// --------------
// Loads the patch.js file for [patch_path], merges [patch_info] onto this
// file, and returns the result.
patch_t patch_init(const char *patch_path, const json_t *patch_info, size_t level);

// Converts a patch to json for runconfig.
// Note that the resulting json doesn't contain all the
// patch fields, only the ones we care about in the
// runconfig file (ie. 'archive' and nothing else).
json_t *patch_to_runconfig_json(const patch_t *patch);

// Free the fields of a patch
void patch_free(patch_t *patch);

// Turns the possibly relative archive path of [patch_info] into an absolute
// one, relative to [base_path].
int patch_rel_to_abs(patch_t *patch_info, const char *base_path);
/// --------------

// Builds a new patch object with an archive directory from a patch description.
patch_t patch_build(const patch_desc_t *sel);

// Builds a patch description from a patch dependency string
patch_desc_t patch_dep_to_desc(const char *dep_str);

/// -----
/// Hooks
/// -----
struct patchhook_t;

// Register a hook function for a certain file extension.
// If patch_size_func is null, a default implementation returning the size of the jdiff file is used instead.
void patchhook_register(const char *ext, func_patch_t patch_func, func_patch_size_t patch_size_func);

// Builds an array of patch hook functions for [fn].
// Has to be free()'d by the caller!
struct patchhook_t* patchhooks_build(const char *fn);

// Loads the jdiff file for a hook, and guess the patched file size.
json_t* patchhooks_load_diff(const struct patchhook_t *hook_array, const char *fn, size_t *size);

// Runs all hook functions in [hook_array] on the given data.
// Returns 1 if one of the hook changed the file in file_inout, and 0 otherwise.
int patchhooks_run(const struct patchhook_t *hook_array, void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch);
/// -----
