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
	auto build_view = runconfig_build_get_view();
	if (!build_view.empty()) {
		const size_t fn_length = PtrDiffStrlen(strchr(fn, '.'), fn);
		return strdup_cat({ fn, fn_length + 1 }, build_view, fn + fn_length);
	}
	else {
		return nullptr;
	}
}

char* fn_for_game(const char *fn)
{
	auto game_id = runconfig_game_get_view();
	if (!game_id.empty()) {
		if (fn) {
			return strdup_cat(game_id, '/', fn);
		}
	}
	else if (fn) {
		return strdup(fn);
	}
	return NULL;
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
	char *patch_fn = NULL;

	if(!patch_info || !patch_info->archive || !fn) {
		return NULL;
	}
	/*if(char archive_end = patch_info->archive[strlen(patch_info->archive) - 1];
		archive_end != '\\' && archive_end != '/') {
		// ZIP archives not yet supported
		return NULL;
	}*/

	patch_fn = strdup_cat(patch_info->archive, '/', fn);
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
	if (file_size) *file_size = 0;
	HANDLE handle = patch_file_stream(patch_info, fn);
	if (handle != INVALID_HANDLE_VALUE) {
		return file_stream_read(handle, file_size);
	}
	return NULL;
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
			if (!json_object_update_recursive(*json_inout, json_new)) {
				json_decref(json_new);
			}
			else {
				*json_inout = json_new;
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
		char* title = strdup_cat("Message from ", patch_info->id);
		log_mbox(title, patch_info->motd_type, patch_info->motd);
		free(title);
	}
	else {
		log_mbox(patch_info->motd_title, patch_info->motd_type, patch_info->motd);
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

static std::unordered_map<std::string_view, patch_val_t> patch_options;

patch_t patch_init(const char *patch_path, const json_t *patch_info, size_t level)
{
	patch_t patch = {};

	if (patch_path == nullptr) {
		return patch;
	}
	if (PathIsRelativeU(patch_path)) {
		// Add the current directory to the patch archive field
		const size_t patch_len = strlen(patch_path) + 1; // Includes + 1 for path separator
		const size_t dir_len = GetCurrentDirectory(0, NULL); // Includes null terminator
		char *full_patch_path = patch.archive = (char *)malloc(patch_len + dir_len);
		GetCurrentDirectory(dir_len, full_patch_path);
		full_patch_path += dir_len;
		full_patch_path[-1] = '/';
		memcpy(full_patch_path, patch_path, patch_len);
	}
	else {
		patch.archive = strdup(patch_path);
	}
	str_slash_normalize(patch.archive);
	patch.config = json_object_get(patch_info, "config");
	// Merge the runconfig patch array and the patch.js
	json_t *patch_js = patch_json_load(&patch, "patch.js", NULL);
	json_object_update_recursive(patch_js, (json_t*)patch_info);

	json_t* id = json_object_get(patch_js, "id");
	if (json_is_string(id)) {
		patch.id = json_string_copy(id);
	} else if (patch.archive) {
		const char* patch_folder = strrchr(patch.archive, '/');
		if (patch_folder && patch_folder != patch.archive) {
			// Don't read backwards past the beginning of the string
			do {
				if (patch_folder[-1] == '/') {
					patch.id = strndup(patch_folder, strlen(patch_folder) - 1);
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
			patch.version = TextInt(sub_versions[3], sub_versions[2], sub_versions[1], sub_versions[0]);
		} else if (json_is_integer(version)) {
			patch.version = (uint32_t)json_integer_value(version);
		}
	}
	if (!patch.version) {
		patch.version = 1;
	}

	if (patch.id) {
		char* patch_test_opt_name = strdup_cat("patch:", patch.id);
		patch_val_t patch_test_opt;
		patch_test_opt.type = PVT_DWORD;
		patch_test_opt.i = patch.version;
		patch_options[patch_test_opt_name] = patch_test_opt;
	}

	patch.title = json_object_get_string_copy(patch_js, "title");
	patch.servers = json_object_get_string_array_copy(patch_js, "servers");
	patch.ignore = json_object_get_string_array_copy(patch_js, "ignore");
	patch.motd = json_object_get_string_copy(patch_js, "motd");
	patch.motd_title = json_object_get_string_copy(patch_js, "motd_title");
	patch.level = level;

	patch.update = json_object_get_eval_bool_default(patch_js, "update", true, JEVAL_NO_EXPRS);

	json_t *dependencies = json_object_get(patch_js, "dependencies");
	if (json_is_array(dependencies)) {
		const size_t array_size = json_array_size(dependencies);
		patch.dependencies = new patch_desc_t[array_size + 1];
		patch.dependencies[array_size].patch_id = NULL;
		json_t *val;
		json_array_foreach_scoped(size_t, i, dependencies, val) {
			patch.dependencies[i] = patch_dep_to_desc(json_string_value(val));
		}
	}

	json_t *fonts = json_object_get(patch_js, "fonts");
	if (json_is_object(fonts)) {
		const size_t array_size = json_object_size(fonts);
		patch.fonts = new char*[array_size + 1];
		patch.fonts[array_size] = NULL;
		const char* font_fn;
		json_t* val;
		size_t i = 0;
		json_object_foreach(fonts, font_fn, val) {
			patch.fonts[i++] = strdup(font_fn);
		}
	}

	patch.motd_type = (DWORD)json_object_get_eval_int_default(patch_js, "motd_type", 0, JEVAL_STRICT | JEVAL_NO_EXPRS);

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
		free((void*)patch->motd);
		free((void*)patch->motd_title);
		if (patch->servers) {
			for (size_t i = 0; patch->servers[i]; i++) {
				free(patch->servers[i]);
			}
			free(patch->servers);
		}
		if (patch->ignore) {
			for (size_t i = 0; patch->ignore[i]; i++) {
				free(patch->ignore[i]);
			}
			free(patch->ignore);
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
			base_dir = strdup(base_path);
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

	size_t diff_size = 0;
	char* diff_fn = strdup_cat(fn, ".jdiff");
	patch = stack_game_json_resolve(diff_fn, &diff_size);
	free(diff_fn);

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
		switch (const uint8_t type_char = tname[0] | 0x20) {
			case 's': case 'w':
				if (const char* opt_str = json_string_value(j_val_val)) {
					const size_t narrow_len = strlen(opt_str) + 1;
					size_t char_size;
					if (tname[1] != '\0') char_size = strtol(tname + 1, nullptr, 10);
					else char_size = (type_char == 's' ? sizeof(char) : sizeof(wchar_t)) * CHAR_BIT;
					switch (char_size) {
						case 8:
							entry.type = PVT_STRING;
							entry.str.len = narrow_len;
							entry.str.ptr = strdup(opt_str);
							break;
						case 16: {
							entry.type = PVT_STRING16;
							entry.str16.len = narrow_len * 2;
							char16_t* str_16 = new char16_t[narrow_len];
							const char* opt_str_end = opt_str + narrow_len;
							size_t c = mbrtoc16(nullptr, nullptr, 0, nullptr);
							for (char16_t* write_str = str_16; c = mbrtoc16(write_str, opt_str, opt_str_end - opt_str, nullptr); ++write_str) {
								if (c > 0) opt_str += c;
								else if (c > -3) break;
							}
							if (c != 0) {
								delete[] str_16;
								log_printf("ERROR: invalid char16 conversion for string16 option %s\n", key);
								continue;
							}
							entry.str16.ptr = str_16;
							break;
						}
						case 32: {
							entry.type = PVT_STRING32;
							entry.str32.len = narrow_len * 4;
							char32_t* str_32 = new char32_t[narrow_len];
							const char* opt_str_end = opt_str + narrow_len;
							size_t c = mbrtoc32(nullptr, nullptr, 0, nullptr);
							for (char32_t* write_str = str_32; c = mbrtoc32(write_str, opt_str, opt_str_end - opt_str, nullptr); ++write_str) {
								if (c > 0) opt_str += c;
								else if (c > -3) break;
							}
							if (c != 0) {
								delete[] str_32;
								log_printf("ERROR: invalid char32 conversion for string32 option %s\n", key);
								continue;
							}
							entry.str32.ptr = str_32;
							break;
						}
						default:
							log_printf("ERROR: invalid char size %s for string option %s\n", tname + 1, key);
							continue;
					}
				}
				else {
					log_printf("ERROR: invalid json type for string option %s\n", key);
					continue;
				}
				break;
			/*case 'c':
				if (json_is_string(j_val_val)) {
					const char* opt_code_str = json_string_value(j_val_val);
					entry.type = PVT_CODE;
					entry.str.ptr = strdup(opt_code_str);
					entry.str.len = binhack_calc_size(opt_code_str);
				}
				else {
					log_printf("ERROR: invalid json type for code option %s\n", key);
					continue;
				}
				break;*/
			case 'i': {
				json_int_t value;
				(void)json_eval_int64(j_val_val, &value, JEVAL_DEFAULT);
				switch (strtol(tname + 1, nullptr, 10)) {
					case 8:
						entry.type = PVT_SBYTE;
						entry.sb = (int8_t)value;
						break;
					case 16:
						entry.type = PVT_SWORD;
						entry.sw = (int16_t)value;
						break;
					case 32:
						entry.type = PVT_SDWORD;
						entry.si = (int32_t)value;
						break;
					case 64:
						entry.type = PVT_SQWORD;
						entry.sq = (int64_t)value;
						break;
					default:
						log_printf("ERROR: invalid integer size %s for option %s\n", tname + 1, key);
						continue;
				}
				break;
			}
			case 'b': case 'u': {
				json_int_t value;
				(void)json_eval_int64(j_val_val, &value, JEVAL_DEFAULT);
				switch (strtol(tname + 1, nullptr, 10)) {
					case 8:
						entry.type = PVT_BYTE;
						entry.b = (uint8_t)value;
						break;
					case 16:
						entry.type = PVT_WORD;
						entry.w = (uint16_t)value;
						break;
					case 32:
						entry.type = PVT_DWORD;
						entry.i = (uint32_t)value;
						break;
					case 64:
						entry.type = PVT_QWORD;
						entry.q = (uint64_t)value;
						break;
					default:
						log_printf("ERROR: invalid integer size %s for option %s\n", tname + 1, key);
						continue;
				}
				break;
			}
			case 'f': {
				double value;
				(void)json_eval_real(j_val_val, &value, JEVAL_DEFAULT);
				switch (strtol(tname + 1, nullptr, 10)) {
					case 32:
						entry.type = PVT_FLOAT;
						entry.f = (float)value;
						break;
					case 64:
						entry.type = PVT_DOUBLE;
						entry.d = value;
						break;
					case 80:
						entry.type = PVT_LONGDOUBLE;
						entry.ld = /*(LongDouble80)*/value;
						break;
					default:
						log_printf("ERROR: invalid float size %s for option %s\n", tname + 1, key);
						continue;
				}
				break;
			}
		}
		const char* op_str = json_object_get_string(j_val, "op");
		patch_val_set_op(op_str, &entry);
		if (const char* slot = strchr(key, '#')) {
			const size_t key_len = PtrDiffStrlen(slot, key) + 1;
			if (patch_val_t* existing_opt = patch_opt_get_len(key, key_len)) {
				if (existing_opt->type == entry.type) {
					patch_val_t result = patch_val_op_str(op_str, *existing_opt, entry);
					if (result.type != PVT_NONE) {
						*existing_opt = result;
					}
					else {
						log_printf("ERROR: invalid operation for option type, values not merged\n");
					}
				}
				else {
					log_printf("ERROR: invalid option types do not match, values not merged\n");
				}
			}
			else {
				patch_options[strdup_size(key, key_len)] = entry;
			}
		}
		patch_options[strdup(key)] = entry;
	}
}

patch_val_t* patch_opt_get(const char *name) {
	std::string_view name_view(name);
	if (patch_options.count(name_view)) {
		return &patch_options[name_view];
	}
	return NULL;
}

patch_val_t* patch_opt_get_len(const char* name, size_t length) {
	std::string_view name_view(name, length);
	if (patch_options.count(name_view)) {
		return &patch_options[name_view];
	}
	return NULL;
}
