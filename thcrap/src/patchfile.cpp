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
	const wchar_t *wildcard;
	func_patch_t patch_func;
	func_patch_size_t patch_size_func;

	constexpr patchhook_t(const wchar_t* wildcard, func_patch_t patch_func, func_patch_size_t patch_size_func)
		: wildcard(wildcard),
		patch_func(patch_func),
		patch_size_func(patch_size_func) {}
};

std::vector<patchhook_t> patchhooks;

HANDLE file_stream(const char *fn)
{
	return CreateFileU(
		fn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL
	);
}

size_t file_stream_size(HANDLE stream) {
#ifndef TH_X64
	return GetFileSize(stream, NULL);
#else
	DWORD ret_high;
	size_t ret = GetFileSize(stream, &ret_high);
	return ret | (size_t)ret_high << 32;
#endif
}

void* file_stream_read(HANDLE stream, size_t *file_size_out)
{
#ifndef TH_X64
	size_t* file_size_ptr = file_size_out ? file_size_out : (size_t*)&file_size_out;
	*file_size_ptr = 0;
	void* ret = NULL;
	if (stream != INVALID_HANDLE_VALUE) {
		if (size_t file_size = GetFileSize(stream, NULL);
			file_size && (ret = malloc(file_size))
		) {
			ReadFile(stream, ret, file_size, (LPDWORD)file_size_ptr, NULL);
		}
		CloseHandle(stream);
	}
	return ret;
#else
	LARGE_INTEGER* file_size_ptr = file_size_out ? (LARGE_INTEGER*)file_size_out : (LARGE_INTEGER*)&file_size_out;
	file_size_ptr->QuadPart = 0;
	void* ret = NULL;
	if (stream != INVALID_HANDLE_VALUE) {
		GetFileSizeEx(stream, file_size_ptr);
		if (size_t file_size = file_size_ptr->QuadPart;
			file_size && (ret = malloc(file_size))
		) {
			uint8_t* ret_writer = (uint8_t*)ret;
			for (; file_size > UINT32_MAX; file_size -= UINT32_MAX) {
				ReadFile(stream, ret_writer, UINT32_MAX, (LPDWORD)file_size_ptr, NULL);
				ret_writer += UINT32_MAX;
			}
			ReadFile(stream, ret_writer, (DWORD)file_size, (LPDWORD)file_size_ptr, NULL);
		}
		CloseHandle(stream);
	}
	return ret;
#endif
}

void* (file_read)(const char *fn, size_t *file_size)
{
	return file_read_inline(fn, file_size);
}

int file_write(const char *fn, const void *file_buffer, size_t file_size)
{
	if unexpected(!fn || !file_buffer || !file_size) {
		return ERROR_INVALID_PARAMETER;
	}

	dir_create_for_fn(fn);

	HANDLE handle = CreateFileU(
		fn, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL
	);
	if unexpected(handle == INVALID_HANDLE_VALUE) {
		return GetLastError();
	}
#ifndef TH_X64
	int ret = W32_ERR_WRAP(WriteFile(handle, file_buffer, file_size, (LPDWORD)&file_size, NULL));
#else
	uint8_t* file_buffer_reader = (uint8_t*)file_buffer;
	DWORD idgaf;
	int ret;
	for (; file_size > UINT32_MAX; file_size -= UINT32_MAX) {
		if unexpected(!WriteFile(handle, file_buffer_reader, UINT32_MAX, &idgaf, NULL)) {
			goto get_error;
		}
		file_buffer_reader += UINT32_MAX;
	}
	if (WriteFile(handle, file_buffer_reader, (DWORD)file_size, &idgaf, NULL)) {
		ret = 0;
	}
	else {
get_error:
		ret = GetLastError();
	}
#endif
	CloseHandle(handle);
	return ret;
}

TH_CALLER_FREE char* fn_for_build(const char *fn)
{
	if (!fn) return nullptr;
	std::string_view build_view = runconfig_build_get_view();
	if (!build_view.empty()) {
		const char* fn_offset = strrchr(fn, '/');
		if(!fn_offset) fn_offset = strrchr(fn, '\\');
		if (!fn_offset) fn_offset = fn;

		if (const char* dot_offset = strchr(fn_offset, '.')) {
			const size_t fn_length = PtrDiffStrlen(dot_offset, fn);
			return strdup_cat({ fn, fn_length + 1 }, build_view, fn + fn_length);
		} else {
			return strdup_cat(fn, ".", build_view);
		}
	}
	return nullptr;
}

TH_CALLER_FREE char* fn_for_game(const char *fn)
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
	if (patch_info && fn) {
		char end_char = patch_info->archive[patch_info->archive_length - 1];

		std::string_view& thcrap_dir = runconfig_thcrap_dir_get_view();

		size_t out_len = 0;
		const char* archive = find_path_substring(patch_info->archive, patch_info->archive_length, thcrap_dir.data(), thcrap_dir.length(), &out_len);

		if (!archive) {
			archive = patch_info->archive;
		}
		else {
			archive += out_len;
		}
			
		log_printf(
			(end_char == '/') || (end_char == '\\') ? "\n%*s+ %s%s" : "\n%*s+ %s/%s"
			, patch_info->level, "", archive, fn
		);
	}
}

int dir_create_for_fn(const char *fn)
{
	int ret = -1;
	if (fn) {
		if (size_t path_length = PtrDiffStrlen(PathFindFileNameU(fn), fn)) {
			char* fn_dir = strdup_size(fn, path_length);
			ret = CreateDirectoryU(fn_dir, NULL);
			free(fn_dir);
		}
		else {
			ret = 0;
		}
	}
	return ret;
}

TH_CALLER_FREE char* fn_for_patch(const patch_t *patch_info, const char *fn)
{
	if unexpected(!patch_info || !patch_info->archive || !fn) {
		return NULL;
	}
	/*if(char archive_end = patch_info->archive[patch_info->archive_length - 1];
		archive_end != '\\' && archive_end != '/') {
		// ZIP archives not yet supported
		return NULL;
	}*/

	char *patch_fn = strdup_cat({ patch_info->archive, patch_info->archive_length }, '/', fn);
	str_slash_normalize(patch_fn);
	return patch_fn;
}

int patch_file_exists(const patch_t *patch_info, const char *fn)
{
	if unexpected(patch_file_blacklisted(patch_info, fn)) {
		return false;
	}
	BOOL ret = FALSE;
	if (char *patch_fn = fn_for_patch(patch_info, fn)) {
		ret = PathFileExistsU(patch_fn);
		free(patch_fn);
	}
	return ret;
}

int patch_file_blacklisted(const patch_t *patch_info, const char *fn)
{
	if unexpected(patch_info->ignore) {
		for (size_t i = 0; patch_info->ignore[i]; i++) {
			if (PathMatchSpecExU(fn, patch_info->ignore[i], PMSF_NORMAL) == S_OK) {
				return 1;
			}
		}
	}
	return 0;
}

HANDLE patch_file_stream(const patch_t *patch_info, const char *fn)
{
	HANDLE ret = INVALID_HANDLE_VALUE;
	if (!patch_file_blacklisted(patch_info, fn)) {
		if (char *patch_fn = fn_for_patch(patch_info, fn)) {
			ret = file_stream(patch_fn);
			free(patch_fn);
		}
	}
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
	int ret = ERROR_INVALID_PARAMETER;
	if (char *patch_fn = fn_for_patch(patch_info, fn)) {
		ret = file_write(patch_fn, file_buffer, file_size);
		free(patch_fn);
	}
	return ret;
}

json_t* patch_json_load(const patch_t *patch_info, const char *fn, size_t *file_size)
{
	if (!patch_file_blacklisted(patch_info, fn)) {
		if (char *_fn = fn_for_patch(patch_info, fn)) {
			json_t* file_json = json_load_file_report_size(_fn, file_size);
			free(_fn);
			return file_json;
		}
	}
	if (file_size) {
		*file_size = 0;
	}
	return NULL;
}

size_t patch_json_merge(json_t **json_inout, const patch_t *patch_info, const char *fn)
{
	size_t file_size = 0;
	if (fn && json_inout) {
		json_t *json_new = patch_json_load(patch_info, fn, &file_size);
		if (json_new) {
			patch_print_fn(patch_info, fn);
			if (!json_object_update_recursive(*json_inout, json_new)) {
				// This is safe to call because there can
				// only be a single reference to json_new
				json_delete(json_new);
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
	if unexpected(!patch_info || !fn || !json) {
		return -1;
	}
	int ret = -1;
	if (char* file_buffer = json_dumps(json, JSON_SORT_KEYS | JSON_INDENT(2))) {
		ret = patch_file_store(patch_info, fn, file_buffer, strlen(file_buffer));
		free(file_buffer);
	}
	return ret;
}

int patch_file_delete(const patch_t *patch_info, const char *fn)
{
	int ret = 1;
	if (char *patch_fn = fn_for_patch(patch_info, fn)) {
		ret = W32_ERR_WRAP(DeleteFileU(patch_fn));
		free(patch_fn);
	}
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
	std::string_view repo_id = desc->repo_id;
	std::string_view patch_id = desc->patch_id;
	static constexpr std::string_view repos = "repos/"sv;
	char* archive = (char*)malloc(repos.length() + repo_id.length() + 1 + patch_id.length() + 2);
	char* archive_write = archive;
	archive_write += repos.copy(archive_write, repos.length());
	archive_write += repo_id.copy(archive_write, repo_id.length());
	*archive_write++ = '/';
	archive_write += patch_id.copy(archive_write, patch_id.length());
	*archive_write++ = '/';
	*archive_write = '\0';
	return patch_t{ archive, PtrDiffStrlen(archive_write, archive) };
}

patch_desc_t patch_dep_to_desc(const char *dep_str)
{
	patch_desc_t desc = {};

	if unexpected(!dep_str) {
		return desc;
	}

	char *dep_buffer = strdup(dep_str);
	if (char *slash = strrchr(dep_buffer, '/')) {
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
		const DWORD dir_len = GetCurrentDirectoryU(0, NULL); // Includes null terminator
		char *full_patch_path = patch.archive = (char *)malloc(patch_len + dir_len);
		GetCurrentDirectoryU(dir_len, full_patch_path);
		full_patch_path += dir_len;
		full_patch_path[-1] = '/';
		memcpy(full_patch_path, patch_path, patch_len);
	}
	else {
		patch.archive = strdup(patch_path);
	}
	str_slash_normalize(patch.archive);
	patch.archive_length = strlen(patch.archive);
	patch.config = json_object_get(patch_info, "config");
	// Merge the runconfig patch array and the patch.js
	json_t *patch_js = patch_json_load(&patch, "patch.js", NULL);
	json_object_update_recursive(patch_js, (json_t*)patch_info);

	json_t* id = json_object_get(patch_js, "id");
	if (json_is_string(id)) {
		patch.id = json_string_copy(id);
	}
	else if (patch.archive) {
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
			union {
				unsigned char sub[4];
				uint32_t combined;
			} version = { 0,0,0,0 };
			int parsed = sscanf(version_string, "%hhu[-.]%hhu[-.]%hhu[-.]%hhu", &version.sub[3], &version.sub[2], &version.sub[1], &version.sub[0]);
			if (parsed != EOF) {
				uint8_t shift = (4 - parsed) * CHAR_BIT;
				patch.version = version.combined >> shift;
			}
		}
		else if (json_is_integer(version)) {
			patch.version = (uint32_t)json_integer_value(version);
		}
	}
	if (!patch.version) {
		patch.version = 1;
	}

	if (patch.id) {
		char* patch_test_opt_name = strdup_cat("patch:", patch.id);
		patch_val_t patch_test_opt = {};
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
		patch.dependencies = (patch_desc_t*)malloc(sizeof(patch_desc_t) * (array_size + 1));
		patch.dependencies[array_size].patch_id = NULL;
		json_t *val;
		json_array_foreach_scoped(size_t, i, dependencies, val) {
			patch.dependencies[i] = patch_dep_to_desc(json_string_value(val));
		}
	}

	json_t *fonts = json_object_get(patch_js, "fonts");
	if (json_is_object(fonts)) {
		const size_t array_size = json_object_size(fonts);
		patch.fonts = (char**)malloc(sizeof(char*) * (array_size + 1));
		patch.fonts[array_size] = NULL;
		const char* font_fn;
		size_t i = 0;
		json_object_foreach_key(fonts, font_fn) {
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
			free(patch->fonts);
		}
		if (patch->dependencies) {
			for (size_t i = 0; patch->dependencies[i].patch_id; i++) {
				free(patch->dependencies[i].patch_id);
				free(patch->dependencies[i].repo_id);
			}
			free(patch->dependencies);
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
	// and is part of... that other half of shlwapi functions that don't work
	// with forward slashes. Since this behavior goes all the way down to
	// PathCanonicalize(), a "proper" reimplementation is not exactly trivial.
	// So we play along for now.
	if(PathIsRelativeA(patch_info->archive)) {
		char * abs_archive;

		size_t archive_len = patch_info->archive_length + 1;

		if (!PathIsRelativeA(base_path)) {
			size_t base_path_len = strlen(base_path) + 1;
			size_t abs_archive_len = base_path_len + archive_len;
			abs_archive = strdup_size(base_path, abs_archive_len);
		}
		else {
			DWORD base_path_len = GetCurrentDirectoryU(0, NULL);
			size_t abs_archive_len = (size_t)base_path_len + strlen(base_path) + 1 + archive_len;
			abs_archive = (char*)malloc(abs_archive_len);
			GetCurrentDirectoryU(base_path_len, abs_archive);
			PathAppendA(abs_archive, base_path);
		}
		str_slash_normalize_win(abs_archive);

		if (!PathIsDirectoryA(base_path)) {
			PathRemoveFileSpecU(abs_archive);
		}

		VLA(char, archive_win, archive_len);
		memcpy(archive_win, patch_info->archive, archive_len);
		str_slash_normalize_win(archive_win);

		PathCombineA(abs_archive, abs_archive, archive_win);
		free(patch_info->archive);
		patch_info->archive = abs_archive;
		patch_info->archive_length = strlen(abs_archive);

		VLA_FREE(archive_win);
		return 0;
	}
	return 1;
}

void patchhook_register(const char *wildcard, func_patch_t patch_func, func_patch_size_t patch_size_func)
{
	wchar_t* wildcard_w = (wchar_t*)utf8_to_utf16(wildcard);
	wstr_slash_normalize(wildcard_w);


	// No checks whether [patch_func] or [patch_size_func] are null
	// pointers here! Some game support code might only want to hook
	// [patch_size_func] to e.g. conveniently run some generic, non-
	// file-related code as early as possible.
	patchhooks.emplace_back(wildcard_w, patch_func, patch_size_func);
}

patchhook_t *patchhooks_build(const char *fn)
{
	if unexpected(!fn) {
		return NULL;
	}
	WCHAR_T_DEC(fn);
	WCHAR_T_CONV(fn);
	wstr_slash_normalize(fn_w);

	patchhook_t *hooks = (patchhook_t *)malloc((patchhooks.size() + 1) * sizeof(patchhook_t));
	patchhook_t *last = std::copy_if(patchhooks.begin(), patchhooks.end(), hooks, [fn_w](const patchhook_t& hook) {
		return PathMatchSpecExW(fn_w, hook.wildcard, PMSF_NORMAL) == S_OK;
	});
	WCHAR_T_FREE(fn);
	last->wildcard = nullptr;

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

	BUILD_VLA_STR(char, diff_fn, fn, ".jdiff");
	size_t diff_size = 0;
	json_t* patch = stack_game_json_resolve(diff_fn, &diff_size);
	VLA_FREE(diff_fn);

	if (size) {
		*size = 0;
		for (size_t i = 0; hook_array[i].wildcard; i++) {
			if (auto func = hook_array[i].patch_size_func) {
				*size += func(fn, patch, diff_size);
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
	// We don't check [patch] here - hooks should be run even if there is no
	// dedicated patch file.
	if unexpected(!file_inout) {
		return -1;
	}
	int ret = 0;
	if (hook_array) {
		for (size_t i = 0; hook_array[i].wildcard; i++) {
			func_patch_t func = hook_array[i].patch_func;
			if (func) {
				if (func(file_inout, size_out, size_in, fn, patch) > 0) {
					const char *patched_files_dump = runconfig_patched_files_dump_get();
					if unexpected(patched_files_dump) {
						DumpDatFile(patched_files_dump, fn, file_inout, size_out, true);
					}
					ret = 1;
				}
			}
		}
	}
	return ret;
}

inline void patch_opt_cleanup(patch_val_t& option) {
	switch (option.type) {
		case PVT_STRING: case PVT_STRING16: case PVT_STRING32: case PVT_CODE:
			free((void*)option.str.ptr);
			break;
	}
}

void patch_opts_clear_all() {
	for (auto& [key, value] : patch_options) {
		patch_opt_cleanup(value);
	}
	patch_options.clear();
}

patch_value_type_t TH_FASTCALL patch_parse_type(const char *type) {
	if unexpected(!type) return PVT_UNKNOWN;
	switch (const uint8_t type_char = type[0] | 0x20) {
		case 's': case 'w':
			size_t char_size;
			if (type[1] != '\0') char_size = strtol(type + 1, nullptr, 10);
			else char_size = (type_char == 's' ? sizeof(char) : sizeof(wchar_t)) * CHAR_BIT;
			switch (char_size) {
				case 8:
					return PVT_STRING;
				case 16:
					return PVT_STRING16;
				case 32:
					return PVT_STRING32;
			}
			break;
		case 'c':
			return PVT_CODE;
		case 'i':
			switch (strtol(type + 1, NULL, 10)) {
				case 8:
				bool_default:
					return PVT_SBYTE;
				case 16:
					return PVT_SWORD;
				case 32:
					return PVT_SDWORD;
				case 64:
					return PVT_SQWORD;
			}
			break;
		case 'b': case 'u': case 'p':
			switch (strtol(type + 1, NULL, 10)) {
				case 8:
					return PVT_BYTE;
				case 16:
					return PVT_WORD;
				case 32:
#ifdef TH_X86
				ptr_default:
#endif
					return PVT_DWORD;
				case 64:
#ifdef TH_X64
				ptr_default:
#endif
					return PVT_QWORD;
				case 0:
					switch (type_char) {
						case 'b': goto bool_default;
						case 'p': goto ptr_default;
					}
					break;
			}
			break;
		case 'f':
			switch (strtol(type + 1, nullptr, 10)) {
				case 32:
					return PVT_FLOAT;
				case 64:
					return PVT_DOUBLE;
				case 80:
					return PVT_LONGDOUBLE;
			}
			break;
	}
	return PVT_UNKNOWN;
}

bool TH_FASTCALL patch_opt_from_raw(patch_value_type_t type, const char* name, void* value) {
	patch_val_t option;
	switch ((option.type = type)) {
		default:
			return false;
		case PVT_SBYTE:
			option.sq = *(int8_t*)value;
			break;
		case PVT_SWORD:
			option.sq = *(int16_t*)value;
			break;
		case PVT_SDWORD:
			option.sq = *(int32_t*)value;
			break;
		case PVT_SQWORD:
			option.sq = *(int64_t*)value;
			break;
		case PVT_BYTE:
			option.q = *(uint8_t*)value;
			break;
		case PVT_WORD:
			option.q = *(uint16_t*)value;
			break;
		case PVT_DWORD:
			option.q = *(uint32_t*)value;
			break;
		case PVT_QWORD:
			option.q = *(uint64_t*)value;
			break;
		case PVT_FLOAT:
			option.f = *(float*)value;
			break;
		case PVT_DOUBLE:
			option.d = *(double*)value;
			break;
		case PVT_LONGDOUBLE:
			option.ld = *(LongDouble80*)value;
			break;
		case PVT_STRING: {
			const char* str = (char*)value;
			size_t length = strlen(str);
			if (const char* new_str = strdup_size(str, length)) {
				option.str.ptr = new_str;
				option.str.len = length + 1;
			}
			else {
				return false;
			}
			break;
		}
		case PVT_STRING16: {
			const char16_t* str = (char16_t*)value;
			size_t length = 0;
			while (str[length++]);
			size_t size = length * sizeof(char16_t);
			if (char16_t* new_str = (char16_t*)malloc(size)) {
				memcpy(new_str, str, size);
				option.str16.ptr = new_str;
				option.str16.len = size;
			} else {
				return false;
			}
			break;
		}
		case PVT_STRING32: {
			const char32_t* str = (char32_t*)value;
			size_t length = 0;
			while (str[length++]);
			size_t size = length * sizeof(char32_t);
			if (char32_t* new_str = (char32_t*)malloc(size)) {
				memcpy(new_str, str, size);
				option.str32.ptr = new_str;
				option.str32.len = size;
			} else {
				return false;
			}
			break;
		}
		case PVT_CODE: {
			if (const char* code_str = strdup((char*)value)) {
				option.code.ptr = code_str;
				option.code.count = 1;
				DisableCodecaveNotFoundWarning(true);
				option.code.len = code_string_calc_size(code_str);
				DisableCodecaveNotFoundWarning(false);
			}
			else {
				return false;
			}
			break;
		}
	}
	if (auto [iter, success] = patch_options.try_emplace({ name }, std::move(option));
		!success
	) {
		patch_opt_cleanup(iter->second);
		iter->second = std::move(option);
	}
	return true;
}

void patch_opts_from_json(json_t *opts) {
	const char *key;
	json_t *j_val;
	json_object_foreach_fast(opts, key, j_val) {
		if unexpected(!json_is_object(j_val)) {
			log_printf("ERROR: invalid parameter for option %s\n", key);
			continue;
		}
		json_t *j_val_val = json_object_get(j_val, "val");
		if unexpected(!j_val_val) {
			continue;
		}
		const char *tname = json_object_get_string(j_val, "type");
		if unexpected(!tname) {
			continue;
		}

		patch_val_t entry;
		switch (entry.type = patch_parse_type(tname)) {
			case PVT_STRING: case PVT_STRING16: case PVT_STRING32: {
				if unexpected(!json_is_string(j_val_val)) {
					log_printf("ERROR: invalid json type for string option %s\n", key);
					continue;
				}
				const char* opt_str = json_string_value(j_val_val);
				const size_t narrow_len = strlen(opt_str) + 1;
				
				switch (entry.type) {
					case PVT_STRING:
						entry.str.len = narrow_len;
						entry.str.ptr = strdup(opt_str);
						break;
					case PVT_STRING16:
						entry.str16.len = narrow_len * sizeof(char16_t);
						if (!(entry.str16.ptr = utf8_to_utf16(opt_str))) {
							log_printf("ERROR: invalid char16 conversion for string16 option %s\n", key);
							continue;
						}
						break;
					case PVT_STRING32:
						entry.str32.len = narrow_len * sizeof(char32_t);
						if (!(entry.str32.ptr = utf8_to_utf32(opt_str))) {
							log_printf("ERROR: invalid char32 conversion for string32 option %s\n", key);
							continue;
						}
						break;
				}
				break;
			}
			case PVT_CODE:
				if (json_is_string(j_val_val) || json_is_array(j_val_val)) {
					if (const char* opt_code_str = json_concat_string_array(j_val_val, key)) {
						entry.type = PVT_CODE;
						entry.code.ptr = opt_code_str;

						// Don't warn about codecave references.
						DisableCodecaveNotFoundWarning(true);
						entry.code.len = code_string_calc_size(opt_code_str);
						DisableCodecaveNotFoundWarning(false);

						entry.code.count = 1;
						break;
					}
				}
				else {
					log_printf("ERROR: invalid json type for code option %s\n", key);
				}
				continue;
			case PVT_SBYTE: case PVT_SWORD: case PVT_SDWORD: case PVT_SQWORD:
			case PVT_BYTE: case PVT_WORD: case PVT_DWORD: case PVT_QWORD:
				jeval64_t value;
				(void)json_eval_int64(j_val_val, &value, JEVAL_DEFAULT);
				switch (entry.type) {
					case PVT_SBYTE:
						entry.sq = (int8_t)value;
						break;
					case PVT_SWORD:
						entry.sq = (int16_t)value;
						break;
					case PVT_SDWORD:
						entry.sq = (int32_t)value;
						break;
					case PVT_SQWORD:
						entry.sq = (int64_t)value;
						break;
					case PVT_BYTE:
						entry.q = (uint8_t)value;
						break;
					case PVT_WORD:
						entry.q = (uint16_t)value;
						break;
					case PVT_DWORD:
						entry.q = (uint32_t)value;
						break;
					case PVT_QWORD:
						entry.q = (uint64_t)value;
						break;
				}
				break;
			case PVT_FLOAT: case PVT_DOUBLE: case PVT_LONGDOUBLE: {
				double value;
				(void)json_eval_real(j_val_val, &value, JEVAL_DEFAULT);
				switch (entry.type) {
					case PVT_FLOAT:
						entry.f = (float)value;
						break;
					case PVT_DOUBLE:
						entry.d = value;
						break;
					case PVT_LONGDOUBLE:
						entry.ld = value;
						break;
				}
				break;
			}
			default:
				log_printf("ERROR: invalid type %s\n", tname);
				continue;
		}

		const char* op_str = json_object_get_string(j_val, "op");
		patch_val_set_op(op_str, &entry);
		if (const char* slot = strchr(key, '#')) {
			if (auto [iter, success] = patch_options.try_emplace({ key, PtrDiffStrlen(slot, key) + 1 }, entry);
				!success
			) {
				if (patch_val_t& existing_opt = iter->second;
					existing_opt.type == entry.type
				) {
					patch_val_t result = patch_val_op_str(op_str, existing_opt, entry);
					if (result.type != PVT_NONE) {
						patch_opt_cleanup(existing_opt);
						existing_opt = std::move(result);
					}
					else {
						patch_opt_cleanup(result);
						log_print("ERROR: invalid operation for option type, values not merged\n");
					}
				}
				else {
					log_print("ERROR: option types do not match, values not merged\n");
				}
			}
		}
		if (auto [iter, success] = patch_options.try_emplace({ key }, std::move(entry));
			!success
		) {
			patch_opt_cleanup(iter->second);
			iter->second = std::move(entry);
		}
	}
}

patch_val_t* patch_opt_get(const char *name) {
	auto existing = patch_options.find(name);
	if (existing != patch_options.end()) {
		return &existing->second;
	}
	return NULL;
}

patch_val_t* patch_opt_get_len(const char* name, size_t length) {
	auto existing = patch_options.find({ name, length });
	if (existing != patch_options.end()) {
		return &existing->second;
	}
	return NULL;
}
