/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Post-processing core updates
  */

#include "thcrap.h"
#include <functional>
#include <string>
#include <vector>

#define THCRAP_CORRUPTED_MSG "You thcrap installation may be corrupted. You can try to redownload it from https://www.thpatch.net/wiki/Touhou_Patch_Center:Download"

static bool do_move(std::vector<std::string>& logs, const char* src, const char* dst);

static bool create_directory_for_path(std::vector<std::string>& logs, const char *path)
{
	char dir[MAX_PATH];
	strcpy(dir, path);
	PathRemoveFileSpecU(dir);
	if (!PathFileExistsU(dir)) {
		// CreateDirectoryU will create the parent directories if they don't exist.
		if (!CreateDirectoryU(dir, nullptr)) {
			logs.emplace_back("[update] Creating directory "s + dir + " failed: "sv + std::to_string(GetLastError()));
			log_mboxf(nullptr, MB_OK, "Update: failed to create directory '%s': %s.\n"
				THCRAP_CORRUPTED_MSG, dir, lasterror_str());
			return false;
		}
		logs.emplace_back("[update] Directory "s + dir + " created."sv);
	}
	else {
		logs.emplace_back("[update] Directory "s + dir + " exists. I don't need to create it."sv);
	}
	return true;
}

static void do_update_repo_paths(const char *run_cfg_fn, const char *old_path, const char *new_path) {
	json_t *run_cfg = json_load_file_report(run_cfg_fn);
	json_t *patches = json_object_get(run_cfg, "patches");

	if (json_is_array(patches)) {
		json_t *patch_info;
		json_array_foreach_scoped(size_t, i, patches, patch_info) {
			const char *archive = json_object_get_string(patch_info, "archive");
			VLA(char, new_archive, strlen(archive) + strlen(new_path) + 1);

			if (!strcmp(old_path, "/")) {
				strcpy(new_archive, new_path);
				strcat(new_archive, archive);
			}
			else {
				size_t old_path_len = strlen(old_path);
				if (strncmp(archive, old_path, old_path_len)) {
					continue;
				}
				strcpy(new_archive, new_path);
				strcat(new_archive, archive + old_path_len - 1);
			}
			json_object_set(patch_info, "archive", json_string(new_archive));
			VLA_FREE(new_archive);
		}
		json_dump_file(run_cfg, run_cfg_fn, JSON_INDENT(2) | JSON_SORT_KEYS);
	}
	json_decref(run_cfg);
	return;
}

/**
  * Because of the mess with the restructuring update, some user might have run the do_update_repo_paths several times.
  * If it happened, we try to fix their config file by removing all the "repos/repos/repos/..." we added, keeping only one of them.
  */
static void do_fix_repo_paths_post_restructuring(const char *run_cfg_fn, const char *broken_path) {
	json_t *run_cfg = json_load_file_report(run_cfg_fn);
	json_t *patches = json_object_get(run_cfg, "patches");

	if (json_is_array(patches)) {
		size_t broken_path_len = strlen(broken_path);
		json_t *patch_info;
		json_array_foreach_scoped(size_t, i, patches, patch_info) {
			const char *archive = json_object_get_string(patch_info, "archive");
			const char *new_archive = archive;

			while (strncmp(new_archive, broken_path, broken_path_len) == 0 &&
				   strncmp(new_archive + broken_path_len, broken_path, broken_path_len) == 0) {
				new_archive += broken_path_len;
			}
			json_object_set(patch_info, "archive", json_string(new_archive));
		}
		json_dump_file(run_cfg, run_cfg_fn, JSON_INDENT(2) | JSON_SORT_KEYS);
	}
	json_decref(run_cfg);
	return;
}

static bool do_move_file(std::vector<std::string>& logs, const char *src, const char *dst)
{
	char full_dst[MAX_PATH];

	if (!PathFileExistsU(src)) {
		logs.emplace_back("[update] "s + src + " doesn't exist. Ignoring."sv);
		// The source file doesn't exist. This is probably an optional file.
		return true;
	}

	if (strchr(dst, '\\') || strchr(dst, '/')) {
		// Move to another directory

		logs.emplace_back("[update] Creating directory for "s + dst + "..."sv);
		if (!create_directory_for_path(logs, dst)) {
			return false;
		}

		strcpy(full_dst, dst);

		const char* basename = src + strlen(src) - 1;
		// Skip backslashs at the end
		while (basename >= src && (*basename == '\\' || *basename == '/')) {
			basename--;
		}
		// Find the last backslash
		while (basename >= src && (*basename != '\\' && *basename != '/')) {
			basename--;
		}
		if (basename >= src) {
			// Found a backslash. Go for the character after it.
			basename++;
		}
		else {
			// Didn't found a backslash.
			basename = src;
		}
		logs.emplace_back("[update] Updating destination with basename "s + basename);

		// PathAppendU is too dumb to deal with forward slashes
		str_slash_normalize_win(full_dst);
		PathAppendU(full_dst, basename);
		dst = full_dst;
		logs.emplace_back("[update] New destination is "s + dst);
	}

	DWORD src_attr = GetFileAttributesA(src);
	DWORD dst_attr = GetFileAttributesA(dst);
	if (src_attr != INVALID_FILE_ATTRIBUTES && dst_attr != INVALID_FILE_ATTRIBUTES &&
		(src_attr & FILE_ATTRIBUTE_DIRECTORY) && (dst_attr & FILE_ATTRIBUTE_DIRECTORY)) {
		// Source and destination are both directories. Merge them.
		logs.emplace_back("[update] Source and destination are both directories. Merge them."sv);
		bool ret = do_move(logs, (std::string(src) + "\\*"sv).c_str(), dst);
		logs.emplace_back("[update] "s + src + "\\* have been moved to "sv + dst + ". Removing the old "sv + src + "..."sv);
		if (RemoveDirectoryU(src)) {
			logs.emplace_back("[update] "s + src + " removed."sv);
		}
		else {
			logs.emplace_back("[update] Removing directory "s + src + " failed: "sv + std::to_string(GetLastError()));
		}
		return ret;
	}

	if (!MoveFileExU(src, dst, MOVEFILE_REPLACE_EXISTING)) {
		logs.emplace_back("[update] Moving "s + src + " to "sv + dst + " failed: "sv + std::to_string(GetLastError()));
		log_mboxf(nullptr, MB_OK, "Update: failed to move '%s' to '%s': %s.\n"
			THCRAP_CORRUPTED_MSG, src, dst, lasterror_str());
		return false;
	}

	return true;
}

static bool do_move(std::vector<std::string>& logs, const char *src, const char *dst)
{
	if (strchr(src, '*') == nullptr) {
		// Simple file
		logs.emplace_back("[update] No wildcard. Moving "s + src + " to "sv + dst + "..."sv);
		return do_move_file(logs, src, dst);
	}
	else {
		// Wildcard
		logs.emplace_back("[update] Wildcard. Moving "s + src + " to "sv + dst + "..."sv);
		WIN32_FIND_DATAA findData;
		HANDLE hFind = FindFirstFileU(src, &findData);
		if (hFind == INVALID_HANDLE_VALUE) {
			if (GetLastError() == ERROR_FILE_NOT_FOUND) {
				// Nothing to move
				logs.emplace_back("[update] "s + src + " doesn't exist. Ignoring..."sv);
				return true;
			} else {
				logs.emplace_back("[update] FindFirstFileU("s + src + ") failed: "sv + std::to_string(GetLastError()));
				log_mboxf(nullptr, MB_OK, "Update: failed to prepare the move of '%s': %s.\n"
					THCRAP_CORRUPTED_MSG, src, lasterror_str());
				return false;
			}
		}
		do {
			if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
				continue;
			}

			char src_file[MAX_PATH];
			strcpy(src_file, src);
			str_slash_normalize_win(src_file);
			if (strchr(src, '\\') || strchr(src, '/')) {
				PathRemoveFileSpecU(src_file);
				PathAppendU(src_file, findData.cFileName);
			}
			else {
				// src doesn't contain a path, just a pattern relative to the current directory.
				// Copy cFileName from the current directory.
				strcpy(src_file, findData.cFileName);
			}
			logs.emplace_back("[update] Moving "s + src_file + " to "sv + dst + "..."sv);
			if (!do_move_file(logs, src_file, dst)) {
				FindClose(hFind);
				return false;
			}
		} while (FindNextFileU(hFind, &findData));
		FindClose(hFind);
		return true;
	}
}

static void for_each_file(const char *wildcard, std::function<void(const char *file)> fn)
{
	if (!strchr(wildcard, '*')) {
		fn(wildcard);
		return;
	}

	BUILD_VLA_STR(char, dir, wildcard);
	PathRemoveFileSpecU(dir);
	PathAddBackslashU(dir);
	str_slash_normalize(dir);
	size_t length = strlen(dir);

	WIN32_FIND_DATAA find_data;
	HANDLE hFind = FindFirstFileU(wildcard, &find_data);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
				continue;
			}

			BUILD_VLA_STR(char, path, dir, length, find_data.cFileName);
			fn(path);
			VLA_FREE(path);
		} while (FindNextFileU(hFind, &find_data));
		FindClose(hFind);
	}
	VLA_FREE(dir);
}

static bool do_update(std::vector<std::string>& logs, json_t *update)
{
	json_t* update_detect = json_object_get(update, "detect");
	if (update_detect) {
		logs.emplace_back("[update] detect field present. Running detect test..."sv);
		json_t* exist = json_object_get(update_detect, "exist");
		if (exist) {
			// If all these files are here, we want to perform the update
			json_t* it;
			json_flex_array_foreach_scoped(size_t, i, exist, it) {
				logs.emplace_back("[update] Detect test - checking if '"s + json_string_value(it) + "' exists."sv);
				if (PathFileExistsU(json_string_value(it)) == FALSE) {
					// File don't exist - cancel the update
					logs.emplace_back("[update] File don't exist - cancelling the update."sv);
					return true;
				}
			}
		}
		json_t* dont_exist = json_object_get(update_detect, "dont_exist");
		if (dont_exist) {
			// If none of these files are here, we want to perform the update
			json_t* it;
			json_flex_array_foreach_scoped(size_t, i, dont_exist, it) {
				logs.emplace_back("[update] Detect test - checking if '"s + json_string_value(it) + "' doesn't exist."sv);
				if (PathFileExistsU(json_string_value(it)) != FALSE) {
					// File exists - cancel the update
					logs.emplace_back("[update] File exists - cancelling the update."sv);
					return true;
				}
			}
		}
		logs.emplace_back("[update] detect field succeeded. Running update..."sv);
	}
	// The detect test passed - apply the update

	json_t* update_delete = json_object_get(update, "delete");
	if (update_delete) {
		logs.emplace_back("[update] delete field present. Running deletion task..."sv);
		json_t* it;
		json_flex_array_foreach_scoped(size_t, i, update_delete, it) {
			// No error checking. Failure to remove a file tend to not be a critical error,
			// it just keeps some clutter around forever.
			const char* file = json_string_value(it);
			logs.emplace_back("[update] Trying to delete "s + file + "..."sv);
			DWORD attr = GetFileAttributesA(file);
			BOOL ret = FALSE;
			if (attr != INVALID_FILE_ATTRIBUTES) {
				if (attr & FILE_ATTRIBUTE_DIRECTORY) {
					ret = RemoveDirectoryU(file);
				}
				else {
					ret = DeleteFileU(file);
				}
			}
			if (ret == FALSE) {
				DWORD error = GetLastError();
				if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
					logs.emplace_back("[update] File doesn't exist. Ignoring..."sv);
				}
				else {
					logs.emplace_back("[update] delete failed. attr="s + std::to_string(attr) + ", GetLastError="sv + std::to_string(GetLastError()));
				}
			}
		}
	}

	json_t* update_move = json_object_get(update, "move");
	if (update_move) {
		logs.emplace_back("[update] move field present. Running move task..."sv);
		const char *key;
		json_t *value;
		json_object_foreach_fast(update_move, key, value) {
			if (!do_move(logs, key, json_string_value(value))) {
				return false;
			}
		}
	}

	json_t* update_repo_paths = json_object_get(update, "update_repo_paths");
	if (update_repo_paths) {
		logs.emplace_back("[update] update_repo_paths field present. Updating run configurations..."sv);
		const char *cfg_files = json_object_get_string(update_repo_paths, "cfg_files");
		const char *old_path = json_object_get_string(update_repo_paths, "old_path");
		const char *new_path = json_object_get_string(update_repo_paths, "new_path");
		if (!cfg_files || !old_path || !new_path) {
			logs.emplace_back("[update] update_repo_paths is missing one or more parameters."sv);
			log_mbox(nullptr, MB_OK, "\"update_repo_paths\" is missing one or more parameters!\n"
				THCRAP_CORRUPTED_MSG);
			return false;
		}

		for_each_file(cfg_files, [&logs, old_path, new_path](const char *cfg_path) {
			logs.emplace_back("[update] Updating "s + cfg_path);
			do_update_repo_paths(cfg_path, old_path, new_path);
		});
	}

	json_t* fix_repo_paths_post_restructuring = json_object_get(update, "fix_repo_paths_post_restructuring");
	if (fix_repo_paths_post_restructuring) {
		logs.emplace_back("[update] fix_repo_paths_post_restructuring field present. Fixing run configurations..."sv);
		const char *cfg_files     = json_object_get_string(fix_repo_paths_post_restructuring, "cfg_files");
		const char *broken_prefix = json_object_get_string(fix_repo_paths_post_restructuring, "broken_prefix");
		if (!cfg_files || !broken_prefix) {
			logs.emplace_back("[update] fix_repo_paths_post_restructuring is missing one or more parameters."sv);
			log_mbox(nullptr, MB_OK, "\"fix_repo_paths_post_restructuring\" is missing one or more parameters!\n"
				THCRAP_CORRUPTED_MSG);
			return false;
		}

		for_each_file(cfg_files, [&logs, broken_prefix](const char *cfg_path) {
			logs.emplace_back("[update] Updating "s + cfg_path);
			do_fix_repo_paths_post_restructuring(cfg_path, broken_prefix);
		});
	}

	return true;
}

static void delete_recursive(std::string path)
{
	DWORD attr = GetFileAttributesA(path.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES) {
		return;
	}
	if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
		DeleteFileU(path.c_str());
		return;
	}

	std::string wildcard = path + "\\*"sv;
	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileU(wildcard.c_str(), &findData);
	if (hFind == INVALID_HANDLE_VALUE) {
		return;
	}
	do {
		if (strcmp(findData.cFileName, ".") != 0 &&
			strcmp(findData.cFileName, "..") != 0) {
			delete_recursive(path + '\\' + findData.cFileName);
		}
	} while (FindNextFileU(hFind, &findData));
	FindClose(hFind);
	RemoveDirectoryU(path.c_str());
}

static void remove_old_versions()
{
	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileU("thcrap_old_*", &findData);
	if (hFind == INVALID_HANDLE_VALUE) {
		return;
	}
	do {
		delete_recursive(findData.cFileName);
	} while (FindNextFileU(hFind, &findData));
	FindClose(hFind);
}

bool update_finalize(std::vector<std::string>& logs)
{
	// Remove the old version. Do that before any code path that can return.
	remove_old_versions();

	json_t *update_list = json_load_file_report("bin\\update.json");
	if (update_list == nullptr) {
		logs.emplace_back("[update] No update file. No update to do."sv);
		return false;
	}

	if (!json_is_array(update_list)) {
		logs.emplace_back("[update] bin\\update.json is not an array."sv);
		log_mbox(nullptr, MB_OK, "Error: bin\\update.json is not an array.\n"
			THCRAP_CORRUPTED_MSG);
		json_decref(update_list);
		return false;
	}

	json_t *update;
	json_array_foreach_scoped(size_t, i, update_list, update) {
		auto i_string = std::to_string(i);
		logs.emplace_back("[update] Running update "sv + i_string + "..."sv);
		if (do_update(logs, update) == false) {
			logs.emplace_back("[update] Update "sv + i_string + " failed"sv);
			log_mbox(nullptr, MB_OK, "An error happened while finalizing the thcrap update!\n"
				THCRAP_CORRUPTED_MSG);
			json_decref(update_list);
			return false;
		}
		logs.emplace_back("[update] Update "sv + i_string + " done."sv);
	}

	json_decref(update_list);
	return true;
}
