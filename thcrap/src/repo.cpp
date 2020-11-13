/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Repository handling.
  */

#include <thcrap.h>
#include "thcrap_update_wrapper.h"
#include <algorithm>

char *RepoGetLocalFN(const char *id)
{
	const char *repo_fn = "repo.js";
	std::string repo = std::string("repos/") + id + "/" + repo_fn;
	return strdup(repo.c_str());
}

repo_t *RepoLoadJson(json_t *repo_js)
{
	if (!json_is_object(repo_js)) {
		return nullptr;
	}
	repo_t *repo = (repo_t*)malloc(sizeof(repo_t));
	memset(repo, 0, sizeof(repo_t));

	auto set_string_if_exist = [repo_js](const char *key, char*& out) {
		json_t *value = json_object_get(repo_js, key);
		if (value) {
			out = strdup(json_string_value(value));
		}
	};
	auto set_array_if_exist = [repo_js](const char *key, char **&out) {
		json_t *array = json_object_get(repo_js, key);
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

	set_string_if_exist("id",       repo->id);
	set_string_if_exist("title",    repo->title);
	set_string_if_exist("contact",  repo->contact);
	set_array_if_exist("servers",   repo->servers);
	set_array_if_exist("neighbors", repo->neighbors);

	json_t *patches = json_object_get(repo_js, "patches");
	if (json_is_object(patches)) {
		std::vector<repo_patch_t> patches_vector;
		const char *patch_id;
		json_t *patch_title;
		json_object_foreach(patches, patch_id, patch_title) {
			patches_vector.push_back({ strdup(patch_id), strdup(json_string_value(patch_title)) });
		}
		repo->patches = new repo_patch_t[patches_vector.size() + 1];
		size_t i = 0;
		for (i = 0; i < patches_vector.size(); i++) {
			repo->patches[i] = patches_vector[i];
		}
		repo->patches[i].patch_id = nullptr;
	}

	return repo;
}

bool RepoWrite(const repo_t *repo)
{
	if (!repo || !repo->id) {
		return false;
	}

	ScopedJson repo_js = json_object();

	// Prepare json file
	json_object_set_new(*repo_js, "id", json_string(repo->id));
	if (repo->title) {
		json_object_set_new(*repo_js, "title", json_string(repo->title));
	}
	if (repo->contact) {
		json_object_set_new(*repo_js, "title", json_string(repo->contact));
	}
	if (repo->servers) {
		json_t *servers = json_array();
		for (size_t i = 0; repo->servers[i]; i++) {
			json_array_append_new(servers, json_string(repo->servers[i]));
		}
		json_object_set_new(*repo_js, "servers", servers);
	}
	if (repo->neighbors) {
		json_t *neighbors = json_array();
		for (size_t i = 0; repo->neighbors[i]; i++) {
			json_array_append_new(neighbors, json_string(repo->neighbors[i]));
		}
		json_object_set_new(*repo_js, "neighbors", neighbors);
	}
	if (repo->patches) {
		json_t *patches = json_object();
		for (size_t i = 0; repo->patches[i].patch_id; i++) {
			json_object_set_new(patches, repo->patches[i].patch_id, json_string(repo->patches[i].title));
		}
		json_object_set_new(*repo_js, "patches", patches);
	}

	// Write json file
	char *repo_fn_local = RepoGetLocalFN(repo->id);
	auto repo_path = std::filesystem::u8path(repo_fn_local);
	auto repo_dir = repo_path;
	repo_dir.remove_filename();
	free(repo_fn_local);

	std::filesystem::create_directories(repo_dir);
	return json_dump_file(*repo_js, repo_path.u8string().c_str(), JSON_INDENT(4)) == 0;
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
			delete[] repo->servers;
		}

		if (repo->neighbors) {
			for (size_t i = 0; repo->neighbors[i]; i++) {
				free(repo->neighbors[i]);
			}
			delete[] repo->neighbors;
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
	repo_t *repo;

	while ((repo = RepoLocalNext(&hFind))) {
		repo_vector.push_back(repo);
	}
	std::sort(repo_vector.begin(), repo_vector.end(), [](repo_t *a, repo_t *b) {
		return strcmp(a->id, b->id) < 0;
	});

	repo_t **repo_array = (repo_t**)malloc(sizeof(repo_t*) * (repo_vector.size() + 1));
	size_t i;
	for (i = 0; i < repo_vector.size(); i++) {
		repo_array[i] = repo_vector[i];
	}
	repo_array[i] = nullptr;
	return repo_array;
}
