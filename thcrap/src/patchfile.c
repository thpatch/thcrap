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

char* game_file_fullname(const char *fn)
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

char* patch_file_fullname(const json_t *patch_info, const char *fn)
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
	patch_fn = patch_file_fullname(patch_info, fn);
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
	patch_fn = patch_file_fullname(patch_info, fn);
	if(!patch_fn) {
		return NULL;
	}
	if(!patch_file_blacklisted(patch_info, fn)) {
		// Got a file!
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

	patch_fn = patch_file_fullname(patch_info, fn);
	if(!patch_fn) {
		return -2;
	}
	CreateDirectory(patch_fn, NULL);

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

	return 0;
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

json_t* stack_json_resolve(const char *fn, size_t *file_size)
{
	json_t *ret = NULL;
	json_t *patch_array;
	json_t *patch_obj;
	size_t i;
	size_t json_size = 0;

	if(!fn) {
		return NULL;
	}
	log_printf("(JSON) Resolving %s... ", fn);

	patch_array = json_object_get(run_cfg, "patches");

	json_array_foreach(patch_array, i, patch_obj) {
		size_t cur_size = 0;
		json_t *cur_json = patch_json_load(patch_obj, fn, &cur_size);

		if(!cur_json) {
			continue;
		}
		{
			const char *archive = json_object_get_string(patch_obj, "archive");
			log_printf("\n%*s+ %s%s", (i + 1), " ", archive, fn);
		}
		json_size += cur_size;
		if(!ret) {
			ret = cur_json;
		} else {
			json_object_merge(ret, cur_json);
			json_decref(cur_json);
		}
	}
	if(!ret) {
		log_printf("not found\n");
	} else {
		log_printf("\n");
	}
	if(file_size) {
		*file_size = json_size;
	}
	return ret;
}

void* stack_game_file_resolve(const char *fn, size_t *file_size)
{
	char *full_fn;
	const char *full_fn_ptr;
	void *ret = NULL;
	int i;
	json_t *patch_array = json_object_get(run_cfg, "patches");

	if(!fn) {
		return NULL;
	}
	full_fn = game_file_fullname(fn);
	// Meh, const correctness.
	full_fn_ptr = full_fn;
	if(!full_fn_ptr) {
		full_fn_ptr = fn;
	}
	// Patch stack has to be traversed backwards
	// because later patches take priority over earlier ones
	for(i = json_array_size(patch_array) - 1; i > -1; i--) {
		ret = patch_file_load(json_array_get(patch_array, i), full_fn_ptr, file_size);
		if(ret) {
			break;
		}
	}
	SAFE_FREE(full_fn);
	return ret;
}

json_t* stack_game_json_resolve(const char *fn, size_t *file_size)
{
	char *full_fn = game_file_fullname(fn);
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
	return json_array_append_new(hook_array, json_integer((int)patch_func));
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
