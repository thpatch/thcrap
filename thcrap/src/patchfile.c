/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Read and write access to the files of a single patch.
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
			fn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
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

int file_write(const char *fn, const void *file_buffer, const size_t file_size)
{
	int ret = -1;
	if(fn && file_buffer && file_size) {
		HANDLE handle;
		DWORD byte_ret;

		dir_create_for_fn(fn);

		EnterCriticalSection(&cs_file_access);
		handle = CreateFile(
			fn, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL
		);
		ret = (handle == INVALID_HANDLE_VALUE);
		if(!ret) {
			WriteFile(handle, file_buffer, file_size, &byte_ret, NULL);
			CloseHandle(handle);
		}
		LeaveCriticalSection(&cs_file_access);
	}
	return ret;
}

char* fn_for_build(const char *fn)
{
	const json_t *build = json_object_get(run_cfg, "build");
	size_t name_len;
	size_t ret_len;
	const char *first_ext = fn;
	char *ret;

	if(!fn || !build) {
		return NULL;
	}
	ret_len = strlen(fn) + 1 + json_string_length(build) + 1;
	ret = (char*)malloc(ret_len);
	if(!ret) {
		return NULL;
	}

	// We need to do this on our own here because the build ID should be placed
	// *before* the first extension.
	while(*first_ext && *first_ext != '.') {
		first_ext++;
	}
	name_len = (first_ext - fn);
	strncpy(ret, fn, name_len);
	sprintf(ret + name_len, ".%s%s", json_string_value(build), first_ext);
	return ret;
}

char* fn_for_game(const char *fn)
{
	const json_t *game_id = json_object_get(run_cfg, "game");
	size_t game_id_len = json_string_length(game_id) + 1;
	char *full_fn;

	if(!fn) {
		return NULL;
	}
	full_fn = (char*)malloc(game_id_len + strlen(fn) + 1);

	full_fn[0] = 0; // Because strcat
	if(game_id) {
		strncpy(full_fn, json_string_value(game_id), game_id_len);
		strcat(full_fn, "/");
	}
	strcat(full_fn, fn);
	return full_fn;
}

void patch_print_fn(const json_t *patch_info, const char *fn)
{
	const json_t *patches = json_object_get(runconfig_get(), "patches");
	const json_t *obj;
	const char *archive;
	size_t level;
	if(!patch_info || !fn) {
		return;
	}
	json_array_foreach(patches, level, obj) {
		if(patch_info == obj) {
			break;
		}
	}
	archive = json_object_get_string(patch_info, "archive");
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
			ret = 0;
		}
	}
	return ret;
}

char* fn_for_patch(const json_t *patch_info, const char *fn)
{
	const json_t *archive = json_object_get(patch_info, "archive");
	size_t archive_len = json_string_length(archive) + 1;
	size_t patch_fn_len;
	char *patch_fn = NULL;

	if(!json_is_string(archive)) {
		return NULL;
	}
	/*
	if(archive[archive_len - 1] != '\\' && archive[archive_len - 1] != '/') {
		// ZIP archives not yet supported
		return NULL;
	}
	*/
	// patch_fn = archive + fn
	patch_fn_len = archive_len + strlen(fn) + 1;
	patch_fn = (char*)malloc(patch_fn_len * sizeof(char));

	strncpy(patch_fn, json_string_value(archive), archive_len);
	strcat(patch_fn, fn);
	str_slash_normalize(patch_fn);
	return patch_fn;
}

int patch_file_exists(const json_t *patch_info, const char *fn)
{
	char *patch_fn = fn_for_patch(patch_info, fn);
	BOOL ret = PathFileExists(patch_fn);
	SAFE_FREE(patch_fn);
	return ret;
}

int patch_file_blacklisted(const json_t *patch_info, const char *fn)
{
	json_t *blacklist = json_object_get(patch_info, "ignore");
	json_t *val;
	size_t i;
	json_array_foreach(blacklist, i, val) {
		const char *wildcard = json_string_value(val);
		if(PathMatchSpec(fn, wildcard)) {
			return 1;
		}
	}
	return 0;
}

void* patch_file_load(const json_t *patch_info, const char *fn, size_t *file_size)
{
	char *patch_fn = fn_for_patch(patch_info, fn);
	void *ret = NULL;

	if(!file_size) {
		return NULL;
	}
	*file_size = 0;
	if(!patch_file_blacklisted(patch_info, fn)) {
		ret = file_read(patch_fn, file_size);
	}
	SAFE_FREE(patch_fn);
	return ret;
}

int patch_file_store(const json_t *patch_info, const char *fn, const void *file_buffer, const size_t file_size)
{
	char *patch_fn = fn_for_patch(patch_info, fn);
	int ret = file_write(patch_fn, file_buffer, file_size);
	SAFE_FREE(patch_fn);
	return ret;
}

json_t* patch_json_load(const json_t *patch_info, const char *fn, size_t *file_size)
{
	size_t json_size;
	void *file_buffer = patch_file_load(patch_info, fn, &json_size);
	json_t *file_json = json_loadb_report(file_buffer, json_size, JSON_DISABLE_EOF_CHECK, fn);

	if(file_size) {
		*file_size = json_size;
	}
	SAFE_FREE(file_buffer);
	return file_json;
}

size_t patch_json_merge(json_t **json_inout, const json_t *patch_info, const char *fn)
{
	size_t file_size = 0;
	if(fn && json_inout) {
		json_t *json_new = patch_json_load(patch_info, fn, &file_size);
		if(json_new) {
			patch_print_fn(patch_info, fn);
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

int patch_file_delete(const json_t *patch_info, const char *fn)
{
	char *patch_fn = fn_for_patch(patch_info, fn);
	int ret = W32_ERR_WRAP(DeleteFile(patch_fn));
	SAFE_FREE(patch_fn);
	return ret;
}

json_t* patch_init(const json_t *patch_info)
{
	json_t *patch_js = patch_json_load(patch_info, "patch.js", NULL);
	return json_object_merge(patch_js, patch_info);
}

int patch_rel_to_abs(json_t *patch_info, const char *base_path)
{
	json_t *archive_obj = json_object_get(patch_info, "archive");
	const char *archive = json_string_value(archive_obj);
	if(!patch_info || !base_path || !archive) {
		return -1;
	}

	// PathCombine() is the only function that reliably works with any kind of
	// relative path (most importantly paths that start with a backslash
	// and are thus relative to the drive root).
	// However, it also considers file names as one implied directory level
	// and is path of... that other half of shlwapi functions that don't work
	// with forward slashes. Since this behavior goes all the way down to
	// PathCanonicalize(), a "proper" reimplementation is not exactly trivial.
	// So we play along for now.
	if(PathIsRelativeA(archive)) {
		STRLEN_DEC(base_path);
		size_t archive_len = json_string_length(archive_obj) + 1;
		size_t abs_archive_len = base_path_len + archive_len;
		VLA(char, abs_archive, abs_archive_len);
		VLA(char, base_dir, base_path_len);
		VLA(char, archive_win, archive_len);

		strncpy(archive_win, archive, archive_len);
		str_slash_normalize_win(archive_win);

		strncpy(base_dir, base_path, base_path_len);
		PathRemoveFileSpec(base_dir);

		PathCombineA(abs_archive, base_dir, archive_win);
		json_string_set(archive_obj, abs_archive);

		VLA_FREE(archive_win);
		VLA_FREE(base_dir);
		VLA_FREE(abs_archive);
		return 0;
	}
	return 1;
}

int patchhook_register(const char *wildcard, func_patch_t patch_func)
{
	json_t *patch_hooks = json_object_get_create(run_cfg, PATCH_HOOKS, JSON_OBJECT);
	json_t *hook_array = json_object_get_create(patch_hooks, wildcard, JSON_ARRAY);
	if(!patch_func) {
		return -1;
	}
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

int patchhooks_run(const json_t *hook_array, void *file_inout, size_t size_out, size_t size_in, json_t *patch, json_t *run_cfg)
{
	json_t *val;
	size_t i;

	// We don't check [patch] here - hooks should be run even if there is no
	// dedicated patch file.
	if(!file_inout) {
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
