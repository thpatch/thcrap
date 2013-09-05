/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Read and write access to the files of a patch.
  */

#include "thcrap.h"

static const char PATCH_HOOKS[] = "patchhooks";

void* file_read(const char *fn, size_t *file_size)
{
	void *ret = NULL;
	size_t file_size_tmp;

	if(!fn) {
		return ret;
	}
	if(!file_size) {
		file_size = &file_size_tmp;
	}
	EnterCriticalSection(&cs_file_access);
	{
		HANDLE handle;
		DWORD byte_ret;

		handle = CreateFile(
			fn, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL
		);
		if(handle != INVALID_HANDLE_VALUE) {
			*file_size = GetFileSize(handle, NULL);
			if(*file_size != 0) {
				ret = malloc(*file_size);
				ReadFile(handle, ret, *file_size, &byte_ret, NULL);
			}
			CloseHandle(handle);
		}
	}
	LeaveCriticalSection(&cs_file_access);
	return ret;
}

char* fn_for_build(const char *fn)
{
	const char *build = json_object_get_string(run_cfg, "build");
	size_t name_len;
	size_t ret_len;
	const char *first_ext;
	char *ret;

	if(!fn || !build) {
		return NULL;
	}
	ret_len = strlen(fn) + 1 + strlen(build) + 1;
	ret = (char*)malloc(ret_len);
	if(!ret) {
		return NULL;
	}

	// We need to do this on our own here because the build ID should be placed
	// *before* the first extension.
	first_ext = fn;
	while(*first_ext && *first_ext != '.') {
		first_ext++;
	}
	name_len = (first_ext - fn);
	strncpy(ret, fn, name_len);
	sprintf(ret + name_len, ".%s%s", build, first_ext);
	return ret;
}

char* fn_for_game(const char *fn)
{
	const char *game_id;
	char *full_fn;

	if(!fn) {
		return NULL;
	}
	game_id = json_object_get_string(run_cfg, "game");
	full_fn = (char*)malloc(strlen(game_id) + 1 + strlen(fn) + 1);

	full_fn[0] = 0; // Because strcat
	if(game_id) {
		strcpy(full_fn, game_id);
		strcat(full_fn, "/");
	}
	strcat(full_fn, fn);
	return full_fn;
}

void log_print_patch_fn(json_t *patch_obj, const char *fn, int level)
{
	const char *archive;
	if(!patch_obj || !fn) {
		return;
	}
	archive = json_object_get_string(patch_obj, "archive");
	log_printf("\n%*s+ %s%s", (level + 1), " ", archive, fn);
}

int dir_create_for_fn(const char *fn)
{
	int ret = -1;
	if(fn) {
		STRLEN_DEC(fn);
		char *fn_dir = PathFindFileNameA(fn);
		if(fn_dir && (fn_dir != fn)) {
			VLA(char, fn_copy, fn_len);
			int fn_pos = fn_dir - fn;

			strncpy(fn_copy, fn, fn_len);
			fn_copy[fn_pos] = '\0';
			ret = CreateDirectory(fn_copy, NULL);
			VLA_FREE(fn_copy);
		} else {
			ret = CreateDirectory(fn, NULL);
		}
	}
	return ret;
}

char* fn_for_patch(const json_t *patch_info, const char *fn)
{
	size_t archive_len;
	const char *archive = NULL;
	size_t patch_fn_len;
	char *patch_fn = NULL;

	archive = json_object_get_string(patch_info, "archive");
	if(!archive) {
		return NULL;
	}
	archive_len = strlen(archive) + 1;
	/*
	if(archive[archive_len - 1] != '\\' && archive[archive_len - 1] != '/') {
		// ZIP archives not yet supported
		return NULL;
	}
	*/
	// patch_fn = archive + fn
	patch_fn_len = archive_len + strlen(fn) + 1;
	patch_fn = (char*)malloc(patch_fn_len * sizeof(char));

	strcpy(patch_fn, archive);
	strcat(patch_fn, fn);
	str_slash_normalize(patch_fn);
	return patch_fn;
}

int patch_file_exists(const json_t *patch_info, const char *fn)
{
	char *patch_fn = NULL;
	BOOL ret;
	if(!patch_info || !fn) {
		return 0;
	}
	patch_fn = fn_for_patch(patch_info, fn);
	ret = PathFileExists(patch_fn);
	SAFE_FREE(patch_fn);
	return ret;
}

int patch_file_blacklisted(const json_t *patch_info, const char *fn)
{
	json_t *blacklist;
	json_t *val;
	size_t i;
	if(!patch_info || !fn) {
		return 0;
	}
	blacklist = json_object_get(patch_info, "ignore");
	json_array_foreach(blacklist, i, val) {
		const char *wildcard = json_string_value(val);
		if(wildcard && PathMatchSpec(fn, wildcard)) {
			return 1;
		}
	}
	return 0;
}

void* patch_file_load(const json_t *patch_info, const char *fn, size_t *file_size)
{
	char *patch_fn = NULL;
	void *ret = NULL;

	if(!patch_info || !fn || !file_size) {
		return NULL;
	}
	*file_size = 0;
	patch_fn = fn_for_patch(patch_info, fn);
	if(!patch_fn) {
		return NULL;
	}
	if(!patch_file_blacklisted(patch_info, fn)) {
		ret = file_read(patch_fn, file_size);
	}
	SAFE_FREE(patch_fn);
	return ret;
}

int patch_file_store(const json_t *patch_info, const char *fn, const void *file_buffer, const size_t file_size)
{
	char *patch_fn = NULL;
	DWORD byte_ret;
	int ret = 0;

	if(!patch_info || !fn || !file_buffer || !file_size) {
		return -1;
	}

	patch_fn = fn_for_patch(patch_info, fn);
	if(!patch_fn) {
		return -2;
	}
	dir_create_for_fn(patch_fn);

	EnterCriticalSection(&cs_file_access);
	{
		HANDLE hFile = CreateFile(patch_fn, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		SAFE_FREE(patch_fn);

		if(hFile != INVALID_HANDLE_VALUE) {
			WriteFile(hFile, file_buffer, file_size, &byte_ret, NULL);
			CloseHandle(hFile);
			ret = 0;
		} else {
			ret = 1;
		}
	}
	LeaveCriticalSection(&cs_file_access);

	return ret;
}

json_t* patch_json_load(const json_t *patch_info, const char *fn, size_t *file_size)
{
	size_t json_size;
	void *file_buffer = NULL;
	json_t *file_json;

	file_buffer = patch_file_load(patch_info, fn, &json_size);
	if(!file_buffer) {
		return NULL;
	}
	if(file_size) {
		*file_size = json_size;
	}
	file_json = json_loadb_report(file_buffer, json_size, JSON_DISABLE_EOF_CHECK, fn);
	SAFE_FREE(file_buffer);
	return file_json;
}

int patch_json_store(const json_t *patch_info, const char *fn, const json_t *json)
{
	char *file_buffer = NULL;
	BOOL ret;

	if(!patch_info || !fn || !json) {
		return -1;
	}
	file_buffer = json_dumps(json, JSON_SORT_KEYS | JSON_INDENT(2));
	ret = patch_file_store(patch_info, fn, file_buffer, strlen(file_buffer));
	SAFE_FREE(file_buffer);
	return ret;
}

// Helper function for stack_json_resolve.
size_t stack_json_load(json_t **json_inout, json_t *patch_obj, const char *fn, size_t level)
{
	size_t file_size = 0;
	if(fn && json_inout) {
		json_t *json_new = patch_json_load(patch_obj, fn, &file_size);
		if(json_new) {
			log_print_patch_fn(patch_obj, fn, level);
			if(!*json_inout) {
				*json_inout = json_new;
			} else {
				json_object_merge(*json_inout, json_new);
				json_decref(json_new);
			}
		}
	}
	return file_size;
}

json_t* stack_json_resolve(const char *fn, size_t *file_size)
{
	char *fn_build = NULL;
	json_t *ret = NULL;
	json_t *patch_array;
	json_t *patch_obj;
	size_t i;
	size_t json_size = 0;

	if(!fn) {
		return NULL;
	}
	fn_build = fn_for_build(fn);
	log_printf("(JSON) Resolving %s... ", fn);

	patch_array = json_object_get(run_cfg, "patches");

	json_array_foreach(patch_array, i, patch_obj) {
		json_size += stack_json_load(&ret, patch_obj, fn, i);
		json_size += stack_json_load(&ret, patch_obj, fn_build, i);
	}
	if(!ret) {
		log_printf("not found\n");
	} else {
		log_printf("\n");
	}
	if(file_size) {
		*file_size = json_size;
	}
	SAFE_FREE(fn_build);
	return ret;
}

void* stack_game_file_resolve(const char *fn, size_t *file_size)
{
	char *fn_common;
	char *fn_build;
	const char *fn_common_ptr;
	void *ret = NULL;
	int i;
	json_t *patch_array = json_object_get(run_cfg, "patches");

	if(!fn) {
		return NULL;
	}

	fn_common = fn_for_game(fn);

	// Meh, const correctness.
	fn_common_ptr = fn_common;
	if(!fn_common_ptr) {
		fn_common_ptr = fn;
	}
	fn_build = fn_for_build(fn_common_ptr);

	log_printf("(Data) Resolving %s... ", fn_common_ptr);
	// Patch stack has to be traversed backwards because later patches take
	// priority over earlier ones, and build-specific files are preferred.
	for(i = json_array_size(patch_array) - 1; i > -1; i--) {
		json_t *patch_obj = json_array_get(patch_array, i);
		const char *log_fn = NULL;
		ret = NULL;

		if(fn_build) {
			ret = patch_file_load(patch_obj, fn_build, file_size);
			log_fn = fn_build;
		}
		if(!ret) {
			ret = patch_file_load(patch_obj, fn_common_ptr, file_size);
			log_fn = fn_common_ptr;
		}
		if(ret) {
			log_print_patch_fn(patch_obj, log_fn, i);
			break;
		}
	}
	SAFE_FREE(fn_common);
	SAFE_FREE(fn_build);
	if(!ret) {
		log_printf("not found\n");
	} else {
		log_printf("\n");
	}
	return ret;
}

json_t* stack_game_json_resolve(const char *fn, size_t *file_size)
{
	char *full_fn = fn_for_game(fn);
	json_t *ret = stack_json_resolve(full_fn, file_size);
	SAFE_FREE(full_fn);
	return ret;
}

int patchhook_register(const char *wildcard, func_patch_t patch_func)
{
	json_t *patch_hooks;
	json_t *hook_array;
	if(!run_cfg || !wildcard || !patch_func) {
		return -1;
	}
	patch_hooks = json_object_get_create(run_cfg, PATCH_HOOKS, json_object());
	hook_array = json_object_get_create(patch_hooks, wildcard, json_array());
	return json_array_append_new(hook_array, json_integer((size_t)patch_func));
}

json_t* patchhooks_build(const char *fn)
{
	json_t *ret = NULL;
	const char *key;
	json_t *val;
	if(!fn) {
		return NULL;
	}
	json_object_foreach(json_object_get(run_cfg, PATCH_HOOKS), key, val) {
		if(PathMatchSpec(fn, key)) {
			if(!ret) {
				ret = json_array();
			}
			json_array_extend(ret, val);
		}
	}
	return ret;
}

int patchhooks_run(const json_t *hook_array, BYTE* file_inout, size_t size_out, size_t size_in, json_t *patch, json_t *run_cfg)
{
	json_t *val;
	size_t i;

	// We don't check [patch] here - hooks should be run even if there is no
	// dedicated patch file.
	if(!hook_array || !file_inout || !run_cfg) {
		return -1;
	}
	json_array_foreach(hook_array, i, val) {
		func_patch_t func = (func_patch_t)json_integer_value(val);
		if(func) {
			func(file_inout, size_out, size_in, patch, run_cfg);
		}
	}
	return 0;
}
