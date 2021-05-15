/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Repository handling.
  */

#include "thcrap.h"
#include "thcrap_update_wrapper.h"
#include <algorithm>

TH_CALLER_FREE char *RepoGetLocalFN(const char *id)
{
	return strdup_cat("repos/", id, "/repo.js");
}

repo_t *RepoLoadJson(json_t *repo_js)
{
	if (!json_is_object(repo_js)) {
		return nullptr;
	}
	repo_t *repo = (repo_t*)malloc(sizeof(repo_t));

	repo->id = json_object_get_string_copy(repo_js, "id");
	repo->title = json_object_get_string_copy(repo_js, "title");
	repo->contact = json_object_get_string_copy(repo_js, "contact");
	repo->servers = json_object_get_string_array_copy(repo_js, "servers");
	repo->neighbors = json_object_get_string_array_copy(repo_js, "neighbors");

	json_t *patches = json_object_get(repo_js, "patches");
	if (json_is_object(patches)) {
		if (size_t patch_count = json_object_size(patches)) {
			repo->patches = new repo_patch_t[patch_count + 1];
			repo->patches[patch_count].patch_id = NULL;

			const char* patch_id;
			json_t* patch_title;
			repo_patch_t* patch = repo->patches;
			json_object_foreach(patches, patch_id, patch_title) {
				patch->patch_id = strdup(patch_id);
				patch->title = json_string_copy(patch_title);
				++patch;
			}
		}
	}
	else {
		repo->patches = NULL;
	}

	return repo;
}

bool RepoWrite(const repo_t *repo)
{
	if (!repo || !repo->id) {
		return false;
	}

	json_t* repo_js = json_object();

	// Prepare json file
	json_object_set_new(repo_js, "id", json_string(repo->id));
	if (repo->title) {
		json_object_set_new(repo_js, "title", json_string(repo->title));
	}
	if (repo->contact) {
		json_object_set_new(repo_js, "contact", json_string(repo->contact));
	}
	if (repo->servers) {
		json_t *servers = json_array();
		for (size_t i = 0; repo->servers[i]; i++) {
			json_array_append_new(servers, json_string(repo->servers[i]));
		}
		json_object_set_new(repo_js, "servers", servers);
	}
	if (repo->neighbors) {
		json_t *neighbors = json_array();
		for (size_t i = 0; repo->neighbors[i]; i++) {
			json_array_append_new(neighbors, json_string(repo->neighbors[i]));
		}
		json_object_set_new(repo_js, "neighbors", neighbors);
	}
	if (repo->patches) {
		json_t *patches = json_object();
		for (size_t i = 0; repo->patches[i].patch_id; i++) {
			json_object_set_new(patches, repo->patches[i].patch_id, json_string(repo->patches[i].title));
		}
		json_object_set_new(repo_js, "patches", patches);
	}

	// Write json file
	char *repo_fn_local = RepoGetLocalFN(repo->id);
	auto repo_path = std::filesystem::u8path(repo_fn_local);
	auto repo_dir = repo_path;
	repo_dir.remove_filename();
	free(repo_fn_local);

	std::filesystem::create_directories(repo_dir);
	int ret = json_dump_file(repo_js, repo_path.u8string().c_str(), JSON_INDENT(4)) == 0;
	json_decref(repo_js);
	return ret;
}

void RepoFree(repo_t *repo)
{
	if (repo) {
		free(repo->id);
		free(repo->title);
		free(repo->contact);

		if (repo->servers) {
			for (size_t i = 0; repo->servers[i]; i++) {
				free(repo->servers[i]);
			}
			free(repo->servers);
		}

		if (repo->neighbors) {
			for (size_t i = 0; repo->neighbors[i]; i++) {
				free(repo->neighbors[i]);
			}
			free(repo->neighbors);
		}

		if (repo->patches) {
			for (size_t i = 0; repo->patches[i].patch_id; i++) {
				free(repo->patches[i].patch_id);
				free(repo->patches[i].title);
			}
			delete[] repo->patches;
		}

		free(repo);
	}
}

repo_t *RepoLocalNext(HANDLE *hFind)
{
	json_t *repo_js = NULL;
	WIN32_FIND_DATAA w32fd;
	BOOL find_ret = 0;
	if(*hFind == NULL) {
		// Too bad we can't do "*/repo.js" or something similar.
		*hFind = FindFirstFile("repos/*", &w32fd);
		if(*hFind == INVALID_HANDLE_VALUE) {
			return NULL;
		}
	} else {
		find_ret = W32_ERR_WRAP(FindNextFile(*hFind, &w32fd));
	}
	while(!find_ret) {
		if(
			(w32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			&& strcmp(w32fd.cFileName, ".")
			&& strcmp(w32fd.cFileName, "..")
		) {
			char *repo_local_fn = RepoGetLocalFN(w32fd.cFileName);
			repo_js = json_load_file_report(repo_local_fn);
			free(repo_local_fn);
			if(repo_js) {
				repo_t *repo = RepoLoadJson(repo_js);
				json_decref(repo_js);
				return repo;
			}
		}
		find_ret = W32_ERR_WRAP(FindNextFile(*hFind, &w32fd));
	};
	FindClose(*hFind);
	return NULL;
}

repo_t **RepoLoad(void)
{
	HANDLE hFind = nullptr;
	std::vector<repo_t*> repo_vector;

	while (repo_t* repo = RepoLocalNext(&hFind)) {
		repo_vector.push_back(repo);
	}
	std::sort(repo_vector.begin(), repo_vector.end(), [](repo_t *a, repo_t *b) {
		return strcmp(a->id, b->id) < 0;
	});

	size_t repo_count = repo_vector.size();
	repo_t **repo_array = (repo_t**)malloc(sizeof(repo_t*) * (repo_count + 1));
	repo_array[repo_count] = nullptr;
	memcpy(repo_array, &repo_vector[0], sizeof(repo_t*) * repo_count);

	return repo_array;
}
