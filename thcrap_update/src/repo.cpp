/**
  * Touhou Community Reliant Automatic Patcher
  * Update module
  *
  * ----
  *
  * Repository handling.
  */

#include <thcrap.h>
#include "repo.h"
#include "update.h"


int RepoDiscoverNeighbors(repo_t *repo, json_t *id_cache, json_t *url_cache, file_write_error_t *fwe_callback)
{
	int ret = 0;
	for (size_t i = 0; repo->neighbors && repo->neighbors[i]; i++) {
		// Recursion!
		ret = RepoDiscoverAtURL(repo->neighbors[i], id_cache, url_cache, fwe_callback);
		if(ret) {
			break;
		}
	}
	return ret;
}

int RepoDiscoverAtServers(servers_t servers, json_t *id_cache, json_t *url_cache, file_write_error_t *fwe_callback)
{
	int ret = 0;
	const char *repo_fn = "repo.js";
	download_ret_t repo_dl;
	char *repo_fn_local = NULL;

	url_cache = json_is_object(url_cache) ? json_incref(url_cache) : json_object();
	id_cache = json_is_object(id_cache) ? json_incref(id_cache) : json_object();

	auto it = servers.begin();
	while(it != servers.end()) {
		if(json_object_get(url_cache, (*it).url)) {
			it = servers.erase(it);
		} else {
			it++;
		}
	}
	if (servers.empty()) {
		return 0;
	}
	auto repo_js = servers.download_valid_json(repo_fn, &repo_dl);
	repo_t *repo = RepoLoadJson(repo_js);

	// Cache all servers that have been visited
	for(auto it : servers) {
		if(it.visited()) {
			json_object_set(url_cache, it.url, json_true());
		}
	}

	// That's all the error checking we need
	if (!repo) {
		return 0;
	}
	if (repo->id) {
		const json_t *old_server = json_object_get(id_cache, repo->id);
		if(!old_server) {
			repo_fn_local = RepoGetLocalFN(repo->id);
			ret = file_write(repo_fn_local, repo_dl.file_buffer, repo_dl.file_size);
			json_object_set(id_cache, repo->id, json_true());
			if(ret) {
				if(fwe_callback && !fwe_callback(repo_fn_local)) {
					goto end;
				}
			}
		} else {
			log_printf("Already got a repository named '%s', ignoring...\n", repo->id);
		}
	} else if(repo) {
		log_printf("Repository file does not specify an ID!\n");
	}

	ret = RepoDiscoverNeighbors(repo, id_cache, url_cache, fwe_callback);
end:
	SAFE_FREE(repo_fn_local);
	SAFE_FREE(repo_dl.file_buffer);
	RepoFree(repo);
	json_decref(repo_js);
	json_decref(url_cache);
	json_decref(id_cache);
	return ret;
}

int RepoDiscoverAtURL(const char *start_url, json_t *id_cache, json_t *url_cache, file_write_error_t *fwe_callback)
{
	servers_t in_servers(start_url);
	return RepoDiscoverAtServers(in_servers, id_cache, url_cache, fwe_callback);
}

int RepoDiscoverFromLocal(json_t *id_cache, json_t *url_cache, file_write_error_t *fwe_callback)
{
	int ret = 0;
	HANDLE hFind = NULL;
	repo_t *repo;
	while (repo = RepoLocalNext(&hFind)) {
		servers_t servers;
		servers.from(repo->servers);
		ret = RepoDiscoverAtServers(servers, id_cache, url_cache, fwe_callback);
		if(!ret) {
			ret = RepoDiscoverNeighbors(repo, id_cache, url_cache, fwe_callback);
		}
		RepoFree(repo);
	}
	return ret;
}
