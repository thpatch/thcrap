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


int RepoDiscoverNeighbors(json_t *repo_js, json_t *id_cache, json_t *url_cache, file_write_error_t *fwe_callback)
{
	int ret = 0;
	json_t *neighbors = json_object_get(repo_js, "neighbors");
	size_t i;
	json_t *neighbor;
	json_array_foreach(neighbors, i, neighbor) {
		const char *neighbor_str = json_string_value(neighbor);
		// Recursion!
		ret = RepoDiscoverAtURL(neighbor_str, id_cache, url_cache, fwe_callback);
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
	const char *id = NULL;
	json_t *repo_fn_local = NULL;
	const char *repo_fn_local_str;

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
	auto repo_js = servers.download_valid_json(repo_fn, &repo_dl);

	// Cache all servers that have been visited
	for(auto it : servers) {
		if(it.visited()) {
			json_object_set(url_cache, it.url, json_true());
		}
	}

	// That's all the error checking we need
	id = json_object_get_string(repo_js, "id");
	if(id) {
		const json_t *old_server = json_object_get(id_cache, id);
		if(!old_server) {
			repo_fn_local = RepoGetLocalFN(id);
			repo_fn_local_str = json_string_value(repo_fn_local);
			ret = file_write(repo_fn_local_str, repo_dl.file_buffer, repo_dl.file_size);
			json_object_set(id_cache, id, json_true());
			if(ret) {
				if(fwe_callback && !fwe_callback(repo_fn_local_str)) {
					goto end;
				}
			}
		} else {
			log_printf("Already got a repository named '%s', ignoring...\n", id);
		}
	} else if(json_is_object(repo_js)) {
		log_printf("Repository file does not specify an ID!\n");
	}

	ret = RepoDiscoverNeighbors(repo_js, id_cache, url_cache, fwe_callback);
end:
	json_decref(repo_fn_local);
	SAFE_FREE(repo_dl.file_buffer);
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
	json_t *repo_js;
	while(repo_js = RepoLocalNext(&hFind)) {
		servers_t servers;
		servers.from(json_object_get(repo_js, "servers"));
		ret = RepoDiscoverAtServers(servers, id_cache, url_cache, fwe_callback);
		if(!ret) {
			ret = RepoDiscoverNeighbors(repo_js, id_cache, url_cache, fwe_callback);
		}
		json_decref(repo_js);
	}
	return ret;
}
