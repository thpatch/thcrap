/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Read and write access to the files of a single patch.
  */

#include "thcrap.h"
#include <algorithm>
#include <list>
#include <vector>

struct patchhook_t
{
	const char *wildcard;
	func_patch_t patch_func;
	func_patch_size_t patch_size_func;
};

std::vector<patchhook_t> patchhooks;

HANDLE file_stream(const char *fn)
{
	return CreateFile(
		fn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL
	);
}

void* file_stream_read(HANDLE stream, size_t *file_size)
{
	void *ret = nullptr;
	size_t file_size_tmp;
	if(!file_size) {
		file_size = &file_size_tmp;
	}
	*file_size = 0;
	if(stream == INVALID_HANDLE_VALUE) {
		return ret;
	}

	DWORD byte_ret;
	*file_size = GetFileSize(stream, nullptr);
	if(*file_size != 0) {
		ret = malloc(*file_size);
		ReadFile(stream, ret, *file_size, &byte_ret, nullptr);
	}
	CloseHandle(stream);
	return ret;
}

void* file_read(const char *fn, size_t *file_size)
{
	return file_stream_read(file_stream(fn), file_size);
}

int file_write(const char *fn, const void *file_buffer, const size_t file_size)
{
	if(!fn || !file_buffer || !file_size) {
		return ERROR_INVALID_PARAMETER;
	}
	DWORD byte_ret;

	dir_create_for_fn(fn);

	auto handle = CreateFile(
		fn, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr
	);
	if(handle == INVALID_HANDLE_VALUE) {
		return GetLastError();
	}
	auto ret = W32_ERR_WRAP(
		WriteFile(handle, file_buffer, file_size, &byte_ret, nullptr
	));
	CloseHandle(handle);
	return ret;
}

char* fn_for_build(const char *fn)
{
	const char *build = runconfig_build_get();
	size_t name_len;
	size_t ret_len;
	const char *first_ext = fn;
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
	const char *game_id = runconfig_game_get();
	if (!game_id) {
		return strdup(fn);
	} else if (!fn) {
		return NULL;
	}
	size_t game_id_len = strlen(game_id) + 1;
	char *full_fn;

	full_fn = (char*)malloc(game_id_len + strlen(fn) + 1);

	full_fn[0] = 0; // Because strcat
	strncpy(full_fn, game_id, game_id_len);
	strcat(full_fn, "/");
	strcat(full_fn, fn);
	return full_fn;
}

void patch_print_fn(const patch_t *patch_info, const char *fn)
{
	const char *archive;
	if(!patch_info || !fn) {
		return;
	}

	archive = patch_info->archive;
	char archive_final_char = archive[strlen(archive) - 1];
	if (archive_final_char != '/' && archive_final_char != '\\') {
		log_printf("\n%*s+ %s/%s", patch_info->level, " ", archive, fn);
	} else {
		log_printf("\n%*s+ %s%s", patch_info->level, " ", archive, fn);
	}
}

int dir_create_for_fn(const char *fn)
{
	int ret = -1;
	if(fn) {
		STRLEN_DEC(fn);
		char *fn_dir = PathFindFileNameU(fn);
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

char* fn_for_patch(const patch_t *patch_info, const char *fn)
{
	size_t patch_fn_len;
	char *patch_fn = NULL;

	if(!patch_info || !patch_info->archive || !fn) {
		return NULL;
	}
	/*
	if(archive[archive_len - 1] != '\\' && archive[archive_len - 1] != '/') {
		// ZIP archives not yet supported
		return NULL;
	}
	*/
	// patch_fn = archive + fn
	patch_fn_len = strlen(patch_info->archive) + 1 + strlen(fn) + 1;
	patch_fn = (char*)malloc(patch_fn_len * sizeof(char));

	strcpy(patch_fn, patch_info->archive);
	PathAddBackslashU(patch_fn);
	strcat(patch_fn, fn);
	str_slash_normalize(patch_fn);
	return patch_fn;
}

int patch_file_exists(const patch_t *patch_info, const char *fn)
{
	if (patch_file_blacklisted(patch_info, fn)) {
		return false;
	}

	char *patch_fn = fn_for_patch(patch_info, fn);
	BOOL ret = PathFileExists(patch_fn);
	SAFE_FREE(patch_fn);
	return ret;
}

int patch_file_blacklisted(const patch_t *patch_info, const char *fn)
{
	if (!patch_info->ignore) {
		return 0;
	}
	for (size_t i = 0; patch_info->ignore[i]; i++) {
		if (PathMatchSpec(fn, patch_info->ignore[i])) {
			return 1;
		}
	}
	return 0;
}

HANDLE patch_file_stream(const patch_t *patch_info, const char *fn)
{
	if(patch_file_blacklisted(patch_info, fn)) {
		return INVALID_HANDLE_VALUE;
	}
	auto *patch_fn = fn_for_patch(patch_info, fn);
	auto ret = file_stream(patch_fn);
	SAFE_FREE(patch_fn);
	return ret;
}

void* patch_file_load(const patch_t *patch_info, const char *fn, size_t *file_size)
{
	return file_stream_read(patch_file_stream(patch_info, fn), file_size);
}

int patch_file_store(const patch_t *patch_info, const char *fn, const void *file_buffer, const size_t file_size)
{
	char *patch_fn = fn_for_patch(patch_info, fn);
	int ret = file_write(patch_fn, file_buffer, file_size);
	SAFE_FREE(patch_fn);
	return ret;
}

json_t* patch_json_load(const patch_t *patch_info, const char *fn, size_t *file_size)
{
	char *_fn = fn_for_patch(patch_info, fn);
	json_t *file_json = json_load_file_report(_fn);

	if(file_size) {
		HANDLE fn_stream = file_stream(_fn);
		if (fn_stream != INVALID_HANDLE_VALUE) {
			*file_size = GetFileSize(fn_stream, NULL);
			CloseHandle(fn_stream);
		}
		else {
			*file_size = 0;
		}
	}
	SAFE_FREE(_fn);
	return file_json;
}

size_t patch_json_merge(json_t **json_inout, const patch_t *patch_info, const char *fn)
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

int patch_json_store(const patch_t *patch_info, const char *fn, const json_t *json)
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

int patch_file_delete(const patch_t *patch_info, const char *fn)
{
	char *patch_fn = fn_for_patch(patch_info, fn);
	int ret = W32_ERR_WRAP(DeleteFile(patch_fn));
	SAFE_FREE(patch_fn);
	return ret;
}

/// Customizable per-patch message on startup
/// -----------------------------------------
void patch_show_motd(const patch_t *patch_info)
{
	if (!patch_info->motd) {
		return;
	}
	if (!patch_info->motd_title) {
		std::string title = std::string("Message from ") + patch_info->id;
		log_mboxf(title.c_str(), patch_info->motd_type, patch_info->motd);
	}
	else {
		log_mboxf(patch_info->motd_title, patch_info->motd_type, patch_info->motd);
	}
}

patch_t patch_build(const patch_desc_t *desc)
{
	std::string archive;
	patch_t patch = {};

	archive = std::string("repos/") + desc->repo_id + "/" + desc->patch_id + "/";
	patch.archive = strdup(archive.c_str());
	return patch;
}

patch_desc_t patch_dep_to_desc(const char *dep_str)
{
	patch_desc_t desc = {};

	if (!dep_str) {
		return desc;
	}

	char *dep_buffer = strdup(dep_str);
	char *slash = strrchr(dep_buffer, '/');
	if (slash) {
		*slash = '\0';
		desc.repo_id = dep_buffer;
		desc.patch_id = strdup(slash + 1);
	}
	else {
		desc.repo_id = nullptr;
		desc.patch_id = dep_buffer;
	}
	return desc;
}

static std::unordered_map<std::string, patch_val_t> patch_options;

patch_t patch_init(const char *patch_path, const json_t *patch_info, size_t level)
{
	patch_t patch = {};

	if (patch_path == nullptr) {
		return patch;
	}
	if (PathIsRelativeU(patch_path)) {
		// Add the current directory to the patch archive field
		size_t full_patch_path_len = strlen(patch_path) + GetCurrentDirectoryU(0, NULL) + 2;
		char *full_patch_path = (char *)malloc(full_patch_path_len);
		GetCurrentDirectoryU(full_patch_path_len, full_patch_path);
		strcpy(PathAddBackslashU(full_patch_path), patch_path);
		str_slash_normalize(full_patch_path);
		patch.archive = full_patch_path;
	}
	else {
		patch.archive = strdup(patch_path);
		str_slash_normalize(patch.archive);
	}
	patch.config = json_object_get(patch_info, "config");
	// Merge the runconfig patch array and the patch.js
	json_t *runconfig_js = json_deep_copy(patch_info);
	json_t *patch_js = patch_json_load(&patch, "patch.js", NULL);
	patch_js = json_object_merge(patch_js, runconfig_js);
	json_decref_safe(runconfig_js);

	auto set_string_if_exist = [patch_js](const char *key, char*& out) {
		json_t *value = json_object_get(patch_js, key);
		if (value) {
			out = strdup(json_string_value(value));
		}
	};
	auto set_array_if_exist = [patch_js](const char *key, char **&out) {
		json_t *array = json_object_get(patch_js, key);
		if (array && json_is_array(array)) {
			out = new char*[json_array_size(array) + 1];
			size_t i = 0;
			json_t *it;
			json_array_foreach(array, i, it) {
				out[i] = strdup(json_string_value(it));
			}
			out[i] = nullptr;
		}
	};

	json_t* id = json_object_get(patch_js, "id");
	if (id && json_is_string(id)) {
		patch.id = strdup(json_string_value(id));
	} else if (patch.archive) {
		const char* patch_folder = strrchr(patch.archive, '/');
		if (patch_folder && patch_folder != patch.archive) {
			// Don't read backwards past the beginning of the string
			do {
				if (patch_folder[-1] == '/') {
					const size_t length = strlen(patch_folder);
					patch.id = (char*)malloc(length);
					strncpy(patch.id, patch_folder, length - 1);
					patch.id[length - 1] = '\0';
					break;
				}
			} while (--patch_folder != patch.archive);
		}
	}

	patch.version = 0;
	json_t* version = json_object_get(patch_js, "version");
	if (version) {
		if (json_is_string(version)) {
			const char* version_string = json_string_value(version);
			char* end_sub_version;
			uint8_t sub_versions[4] = { 0 };
			for (int i = 0; i < 4; ++i) {
				sub_versions[i] = (uint8_t)strtoul(version_string, &end_sub_version, 10);
				for (; end_sub_version[0] && !isdigit(end_sub_version[0]); ++end_sub_version);
				if (!end_sub_version[0]) break;
				version_string = end_sub_version;
			}
			patch.version = sub_versions[0] << 24 | sub_versions[1] << 16 | sub_versions[2] << 8 | sub_versions[3];
		} else if (json_is_integer(version)) {
			patch.version = (uint32_t)json_integer_value(version);
		}
	}
	if (!patch.version) {
		patch.version = 1;
	}

	if (patch.id) {
		char* patch_test_opt_name = (char*)malloc(strlen(patch.id) + 7);
		strcpy(patch_test_opt_name, "patch:");
		strcat(patch_test_opt_name, patch.id);
		patch_val_t patch_test_opt;
		patch_test_opt.type = VT_DWORD;
		patch_test_opt.i = patch.version;
		patch_options[patch_test_opt_name] = patch_test_opt;
		free(patch_test_opt_name);
	}

	set_string_if_exist("title",      patch.title);
	set_array_if_exist( "servers",    patch.servers);
	set_array_if_exist( "ignore",     patch.ignore);
	set_string_if_exist("motd",       patch.motd);
	set_string_if_exist("motd_title", patch.motd_title);
	patch.level = level;

	patch.update = true;
	json_t *update = json_object_get(patch_js, "update");
	if (update && json_is_false(update)) {
		patch.update = false;
	}

	json_t *dependencies = json_object_get(patch_js, "dependencies");
	if (json_is_array(dependencies)) {
		patch.dependencies = new patch_desc_t[json_array_size(dependencies) + 1];
		size_t i;
		json_t *val;
		json_array_foreach(dependencies, i, val) {
			patch.dependencies[i] = patch_dep_to_desc(json_string_value(val));
		}
		patch.dependencies[i].patch_id = nullptr;
	}

	json_t *fonts = json_object_get(patch_js, "fonts");
	if (json_is_object(fonts)) {
		std::vector<char*> fonts_vector;
		const char *font_fn;
		json_t *val;
		json_object_foreach(fonts, font_fn, val) {
			fonts_vector.push_back(strdup(font_fn));
		}
		patch.fonts = new char*[fonts_vector.size() + 1];
		size_t i = 0;
		for (char *it : fonts_vector) {
			patch.fonts[i] = it;
			i++;
		}
		patch.fonts[i] = nullptr;
	}

	json_t *motd_type = json_object_get(patch_js, "motd_type");
	if (motd_type && json_is_integer(motd_type)) {
		patch.motd_type = (DWORD)json_integer_value(motd_type);
	}

	json_decref(patch_js);
	return patch;
}

json_t *patch_to_runconfig_json(const patch_t *patch)
{
	return json_pack("{s:s}", "archive", patch->archive);
}

void patch_free(patch_t *patch)
{
	if (patch) {
		free(patch->archive);
		free(patch->id);
		free(patch->title);
		free(patch->motd);
		free(patch->motd_title);
		if (patch->servers) {
			for (size_t i = 0; patch->servers[i]; i++) {
				free(patch->servers[i]);
			}
			delete[] patch->servers;
		}
		if (patch->ignore) {
			for (size_t i = 0; patch->ignore[i]; i++) {
				free(patch->ignore[i]);
			}
			delete[] patch->ignore;
		}
		if (patch->fonts) {
			for (size_t i = 0; patch->fonts[i]; i++) {
				free(patch->fonts[i]);
			}
			delete[] patch->fonts;
		}
		if (patch->dependencies) {
			for (size_t i = 0; patch->dependencies[i].patch_id; i++) {
				free(patch->dependencies[i].patch_id);
				free(patch->dependencies[i].repo_id);
			}
			delete[] patch->dependencies;
		}
	}
}

int patch_rel_to_abs(patch_t *patch_info, const char *base_path)
{
	if(!patch_info || !base_path || !patch_info->archive) {
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
	if(PathIsRelativeA(patch_info->archive)) {
		size_t base_path_len;
		char *base_dir;

		if (!PathIsRelativeA(base_path)) {
			base_path_len = strlen(base_path) + 1;
			base_dir = (char*)malloc(base_path_len);
			strncpy(base_dir, base_path, base_path_len);
		}
		else {
			base_path_len = GetCurrentDirectory(0, NULL) + strlen(base_path) + 1;
			base_dir = (char*)malloc(base_path_len);
			GetCurrentDirectory(base_path_len, base_dir);
			PathAppendA(base_dir, base_path);
		}
		str_slash_normalize_win(base_dir);

		if (!PathIsDirectoryA(base_path)) {
			PathRemoveFileSpec(base_dir); 
		}

		size_t archive_len = strlen(patch_info->archive) + 1;
		size_t abs_archive_len = base_path_len + archive_len;
		VLA(char, archive_win, archive_len);
		char *abs_archive = (char*)malloc(abs_archive_len);

		strncpy(archive_win, patch_info->archive, archive_len);
		str_slash_normalize_win(archive_win);

		PathCombineA(abs_archive, base_dir, archive_win);
		patch_info->archive = abs_archive;

		free(base_dir);
		VLA_FREE(archive_win);
		return 0;
	}
	return 1;
}

void patchhook_register(const char *wildcard, func_patch_t patch_func, func_patch_size_t patch_size_func)
{
	char *wildcard_normalized = strdup(wildcard);
	str_slash_normalize(wildcard_normalized);

	// No checks whether [patch_func] or [patch_size_func] are null
	// pointers here! Some game support code might only want to hook
	// [patch_size_func] to e.g. conveniently run some generic, non-
	// file-related code as early as possible.
	patchhook_t hook = {};
	hook.wildcard = wildcard_normalized;
	hook.patch_func = patch_func;
	hook.patch_size_func = patch_size_func;
	patchhooks.push_back(hook);
}

patchhook_t *patchhooks_build(const char *fn)
{
	if(!fn) {
		return NULL;
	}
	VLA(char, fn_normalized, strlen(fn) + 1);
	strcpy(fn_normalized, fn);
	str_slash_normalize(fn_normalized);

	patchhook_t *hooks = (patchhook_t *)malloc((patchhooks.size() + 1) * sizeof(patchhook_t));
	patchhook_t *last = std::copy_if(patchhooks.begin(), patchhooks.end(), hooks, [fn_normalized](const patchhook_t& hook) {
		return PathMatchSpec(fn_normalized, hook.wildcard);
	});
	last->wildcard = nullptr;
	VLA_FREE(fn_normalized);

	if (hooks[0].wildcard == nullptr) {
		free(hooks);
		return nullptr;
	}
	return hooks;
}

json_t *patchhooks_load_diff(const patchhook_t *hook_array, const char *fn, size_t *size)
{
	if (!hook_array || !fn) {
		return nullptr;
	}
	json_t *patch;

	size_t diff_fn_len = strlen(fn) + strlen(".jdiff") + 1;
	size_t diff_size = 0;
	VLA(char, diff_fn, diff_fn_len);
	strcpy(diff_fn, fn);
	strcat(diff_fn, ".jdiff");
	patch = stack_game_json_resolve(diff_fn, &diff_size);
	VLA_FREE(diff_fn);

	if (size) {
		*size = 0;
		for (size_t i = 0; hook_array[i].wildcard; i++) {
			if (hook_array[i].patch_size_func) {
				*size += hook_array[i].patch_size_func(fn, patch, diff_size);
			}
			else {
				*size += diff_size;
			}
		}
	}

	return patch;
}

int patchhooks_run(const patchhook_t *hook_array, void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
	int ret;

	// We don't check [patch] here - hooks should be run even if there is no
	// dedicated patch file.
	if(!file_inout) {
		return -1;
	}
	ret = 0;
	for (size_t i = 0; hook_array && hook_array[i].wildcard; i++) {
		func_patch_t func = hook_array[i].patch_func;
		if(func) {
			if (func(file_inout, size_out, size_in, fn, patch) > 0) {
				ret = 1;
			}
		}
	}
	return ret;
}

void patch_opts_from_json(json_t *opts) {
	const char *key;
	json_t *j_val;
	json_object_foreach(opts, key, j_val) {
		if (!json_is_object(j_val)) {
			log_printf("ERROR: invalid parameter for option %s\n", key);
			continue;
		}
		json_t *j_val_val = json_object_get(j_val, "val");
		if (!j_val_val) {
			continue;
		}
		const char *tname = json_object_get_string(j_val, "type");
		if (!tname) {
			continue;
		}
		patch_val_t entry;
		switch (tname[0] & 0xDF) {
			case 'W':
				if (json_is_string(j_val_val)) {
					const char* opt_str = json_string_value(j_val_val);
					entry.type = VT_WSTRING;
					const size_t length = strlen(opt_str) + 1;
					wchar_t* wide_str = new wchar_t[length];
					swprintf(wide_str, length, L"%hs", opt_str);
					entry.wstr.ptr = wide_str;
					entry.wstr.len = length;
				}
				else {
					log_printf("ERROR: invalid json type for wide string option %s\n", key);
					continue;
				}
				break;
			case 'S':
				if (json_is_string(j_val_val)) {
					const char* opt_str = json_string_value(j_val_val);
					entry.type = VT_STRING;
					entry.str.ptr = strdup(opt_str);
					entry.str.len = strlen(opt_str) + 1;
				}
				else {
					log_printf("ERROR: invalid json type for string option %s\n", key);
					continue;
				}
				break;
			/*case 'C':
				if (json_is_string(j_val_val)) {
					const char* opt_code_str = json_string_value(j_val_val);
					entry.type = VT_CODE;
					entry.str.ptr = strdup(opt_code_str);
					entry.str.len = binhack_calc_size(opt_code_str);
				}
				else {
					log_printf("ERROR: invalid json type for code option %s\n", key);
					continue;
				}
				break;*/
			case 'I': {
				size_t value;
				json_eval_int(j_val_val, &value, JEVAL_DEFAULT);
				switch (strtol(tname + 1, nullptr, 10)) {
					case 8:
						entry.type = VT_SBYTE;
						entry.sb = (int8_t)value;
						break;
					case 16:
						entry.type = VT_SWORD;
						entry.sw = (int16_t)value;
						break;
					case 32:
						entry.type = VT_SDWORD;
						entry.si = (int32_t)value;
						break;
					/*case 64:
						entry.type = VT_SQWORD;
						entry.sq = (int64_t)value;
						break;*/
					default:
						log_printf("ERROR: invalid integer size %s for option %s\n", tname, key);
						continue;
				}
				break;
			}
			case 'B': case 'U': {
				size_t value;
				json_eval_int(j_val_val, &value, JEVAL_DEFAULT);
				switch (strtol(tname + 1, nullptr, 10)) {
					case 8:
						entry.type = VT_BYTE;
						entry.b = (uint8_t)value;
						break;
					case 16:
						entry.type = VT_WORD;
						entry.w = (uint16_t)value;
						break;
					case 32:
						entry.type = VT_DWORD;
						entry.i = (uint32_t)value;
						break;
					/*case 64:
						entry.type = VT_QWORD;
						entry.q = (uint64_t)value;
						break;*/
					default:
						log_printf("ERROR: invalid integer size %s for option %s\n", tname, key);
						continue;
				}
				break;
			}
			case 'F': {
				double value;
				json_eval_real(j_val_val, &value, JEVAL_DEFAULT);
				switch (strtol(tname + 1, nullptr, 10)) {
					case 32:
						entry.type = VT_FLOAT;
						entry.f = (float)value;
						break;
					case 64:
						entry.type = VT_DOUBLE;
						entry.d = value;
						break;
					default:
						log_printf("ERROR: invalid float type %s for option %s\n", tname, key);
						continue;
				}
				break;
			}
		}
		patch_options[key] = entry;
	}
}

patch_val_t* patch_opt_get(const char *name) {
	auto val = patch_options.find(name);
	if (val == patch_options.end()) {
		return NULL;
	}
	return &val->second;
}
