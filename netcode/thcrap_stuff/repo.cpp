/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Repository handling.
  */

#include "thcrap.h"
#include <algorithm>
#include <filesystem>
#include <cstring>

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

void repo_foreach(std::function<void(repo_t*)> callback)
{
    for (auto& it : std::filesystem::directory_iterator("repos/")) {
        if (!it.is_directory()) {
            continue;
        }
        char *repo_local_fn = RepoGetLocalFN(it.path().filename().u8string().c_str());
        json_t *repo_js = json_load_file(repo_local_fn, 0, nullptr);
		free(repo_local_fn);
        if (repo_js) {
			repo_t *repo = RepoLoadJson(repo_js);
            callback(repo);
        }
        json_decref(repo_js);
    }
}

repo_t **RepoLoad(void)
{
	std::vector<repo_t*> repo_vector;
	repo_t *repo;

	repo_foreach([&repo_vector](repo_t *repo) {
		repo_vector.push_back(repo);
	});
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
