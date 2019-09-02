/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Post-processing core updates
  */

#include "thcrap.h"

#define THCRAP_CORRUPTED_MSG "You thcrap installation may be corrupted. You can try to redownload it from https://www.thpatch.net/wiki/Touhou_Patch_Center:Download"

static bool create_directory_for_path(const char *path)
{
	char dir[MAX_PATH];
	strcpy(dir, path);
	PathRemoveFileSpecU(dir);
	if (!PathFileExistsU(dir)) {
		// CreateDirectoryU will create the parent directories if they don't exist.
		if (!CreateDirectoryU(dir, nullptr)) {
			log_mboxf(nullptr, MB_OK, "Update: failed to create directory %s.\n"
				THCRAP_CORRUPTED_MSG, dir);
			return false;
		}
	}
	return true;
}

static void do_update_repo_paths(const char *run_cfg_fn, const char *old_path, const char *new_path) {
	json_t *run_cfg = json_load_file_report(run_cfg_fn);
	json_t *patches = json_object_get(run_cfg, "patches");

	if (!json_is_array(patches)) {
		json_decref(run_cfg);
		return;
	}

	size_t i;
	json_t *patch_info;
	json_array_foreach(patches, i, patch_info) {
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
	json_decref(run_cfg);
	return;
}

static bool do_move_file(const char *src, const char *dst)
{
	if (!PathFileExistsU(src)) {
		// The source file doesn't exist. This is probably an optional file.
		return true;
	}

	if (strchr(dst, '\\') || strchr(dst, '/')) {
		// Move to another directory

		if (!create_directory_for_path(dst)) {
			return false;
		}

		char full_dst[MAX_PATH];
		strcpy(full_dst, dst);
		// PathAppendU is too dumb to deal with forward slashes
		str_slash_normalize_win(full_dst);
		PathAppendU(full_dst, src);
		if (!MoveFileU(src, full_dst)) {
			log_mboxf(nullptr, MB_OK, "Update: failed to move %s to %s.\n"
				THCRAP_CORRUPTED_MSG, src, dst);
			return false;
		}
	}
	else {
		// Rename a file
		if (!MoveFileU(src, dst)) {
			log_mboxf(nullptr, MB_OK, "Update: failed to rename %s to %s.\n"
				THCRAP_CORRUPTED_MSG, src, dst);
			return false;
		}
	}
	return true;
}

static bool do_move(const char *src, const char *dst)
{
	// If the thing moved is a directory, it's a patch repo. The destination is stored here for when the run configuration get's updated

	if (strchr(src, '*') == nullptr) {
		// Simple file
		return do_move_file(src, dst);
	}
	else {
		// Wildcard
		WIN32_FIND_DATAA findData;
		HANDLE hFind = FindFirstFileU(src, &findData);
		if (hFind == INVALID_HANDLE_VALUE) {
			if (GetLastError() == ERROR_FILE_NOT_FOUND) {
				// Nothing to move
				return true;
			} else {
				log_mboxf(nullptr, MB_OK, "Update: failed to prepare the move of %s.\n"
					THCRAP_CORRUPTED_MSG, src);
				return false;
			}
		}
		do {
			if (!do_move_file(findData.cFileName, dst)) {
				FindClose(hFind);
				return false;
			}
		} while (FindNextFileU(hFind, &findData));
		FindClose(hFind);
		return true;
	}
}

static bool do_update(json_t *update)
{
	json_t *update_detect     = json_object_get(update, "detect");
	json_t *update_delete     = json_object_get(update, "delete");
	json_t *update_move       = json_object_get(update, "move");
	json_t *update_repo_paths = json_object_get(update, "update_repo_paths");

	if (update_detect) {
		json_t *exist = json_object_get(update_detect, "exist");
		if (exist) {
			// If all these files are here, we want to perform the update
			size_t i;
			json_t *it;
			json_flex_array_foreach(exist, i, it) {
				if (PathFileExistsU(json_string_value(it)) == FALSE) {
					// File don't exist - cancel the update
					return true;
				}
			}
		}
	}
	// The detect test passed - apply the update

	if (update_delete) {
		size_t i;
		json_t *it;
		json_flex_array_foreach(update_delete, i, it) {
			// No error checking. Failure to remove a file tend to not be a critical error,
			// it just keeps some clutter around forever.
			DeleteFileU(json_string_value(it));
		}
	}

	if (update_move) {
		const char *key;
		json_t *value;
		json_object_foreach(update_move, key, value) {
			if (!do_move(key, json_string_value(value))) {
				return false;
			}
		}
	}

	if (update_repo_paths) {
		const char *cfg_files = json_object_get_string(update_repo_paths, "cfg_files");
		const char *old_path = json_object_get_string(update_repo_paths, "old_path");
		const char *new_path = json_object_get_string(update_repo_paths, "new_path");
		if (!cfg_files | !old_path | !new_path) {
			log_mbox(nullptr, MB_OK, "\"update_repo_paths\" is missing one or more parameters!\n"
				THCRAP_CORRUPTED_MSG);
			return false;
		}

		if (!strchr(cfg_files, '*')) {
			do_update_repo_paths(cfg_files, old_path, new_path);
		}
		else {
			VLA(char, run_cfg_dir, strlen(cfg_files));
			strcpy(run_cfg_dir, cfg_files);
			PathRemoveFileSpecU(run_cfg_dir);
			PathAddBackslashU(run_cfg_dir);
			str_slash_normalize(run_cfg_dir);
			size_t run_cfg_dir_len = strlen(run_cfg_dir);
			WIN32_FIND_DATAA find_data;
			HANDLE hFind = FindFirstFileU(cfg_files, &find_data);
			do {
				VLA(char, run_cfg_fn, run_cfg_dir_len + 1 + strlen(find_data.cFileName));
				strcpy(run_cfg_fn, run_cfg_dir);
				strcat(run_cfg_fn, find_data.cFileName);
				do_update_repo_paths(run_cfg_fn, old_path, new_path);
				VLA_FREE(run_cfg_fn);
			} while (FindNextFileU(hFind, &find_data));
			FindClose(hFind);
		}
	}

	return true;
}

bool update_finalize()
{
	json_t *update_list = json_load_file_report("bin\\update.json");
	if (update_list == nullptr) {
		return false;
	}

	if (!json_is_array(update_list)) {
		log_mbox(nullptr, MB_OK, "Error: bin\\update.json is not an array.\n"
			THCRAP_CORRUPTED_MSG);
		json_decref(update_list);
		return false;
	}

	size_t i;
	json_t *update;
	json_array_foreach(update_list, i, update) {
		if (do_update(update) == false) {
			log_mbox(nullptr, MB_OK, "An error happened while finalizing the thcrap update!\n"
				THCRAP_CORRUPTED_MSG);
			json_decref(update_list);
			return false;
		}
	}

	json_decref(update_list);
	return true;
}
