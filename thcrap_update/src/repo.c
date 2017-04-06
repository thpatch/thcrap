/**
  * Touhou Community Reliant Automatic Patcher
  * Update Plugin
  *
  * ----
  *
  * Repository handling.
  */

#include <thcrap.h>
#include "repo.h"
#include "update.h"


int RepoDiscoverNeighbors(json_t *repo_js, json_t *id_cache, json_t *url_cache)
{
	int ret = 0;
	json_t *neighbors = json_object_get(repo_js, "neighbors");
	size_t i;
	json_t *neighbor;
	json_array_foreach(neighbors, i, neighbor) {
		const char *neighbor_str = json_string_value(neighbor);
		// Recursion!
		ret = RepoDiscoverAtURL(neighbor_str, id_cache, url_cache);
		if(ret) {
			break;
		}
	}
	return ret;
}

int RepoDiscoverAtServers(json_t *servers, json_t *id_cache, json_t *url_cache)
{
	int ret = 0;
	const char *repo_fn = "repo.js";
	json_t *in_mirrors = json_copy(servers);
	size_t i;
	json_t *server;
	DWORD repo_size;
	void *repo_buffer = NULL;
	json_t *repo_js = NULL;
	const char *id = NULL;
	json_t *repo_fn_local = NULL;
	const char *repo_fn_local_str;

	url_cache = json_is_object(url_cache) ? json_incref(url_cache) : json_object();
	id_cache = json_is_object(id_cache) ? json_incref(id_cache) : json_object();

	json_array_foreach(in_mirrors, i, server) {
		const char *server_url = json_object_get_string(server, "url");
		if(json_object_get(url_cache, server_url)) {
			json_array_remove(in_mirrors, i);
			i--;
		}
	}
	repo_buffer = ServerDownloadFile(in_mirrors, repo_fn, &repo_size, NULL);
	if(repo_buffer) {
		repo_js = json_loadb_report(repo_buffer, repo_size, 0, repo_fn);
	}
	// Cache all servers that have been visited
	json_array_foreach(in_mirrors, i, server) {
		json_t *server_time = json_object_get(server, "time");
		if(!(json_integer_value(server) == 1)) {
			const char *server_url = json_object_get_string(server, "url");
			json_object_set(url_cache, server_url, json_true());
		}
	}

	// That's all the error checking we need
	id = json_object_get_string(repo_js, "id");
	if(id) {
		const json_t *old_server = json_object_get(id_cache, id);
		if(!old_server) {
			repo_fn_local = RepoGetLocalFN(id);
			repo_fn_local_str = json_string_value(repo_fn_local);
			ret = file_write(repo_fn_local_str, repo_buffer, repo_size);
			json_object_set(id_cache, id, json_true());
			if(ret) {
				log_printf(
					"\n"
					"Error writing to %s!\n"
					"You probably do not have the permission to write to the current directory,\n"
					"or the file itself is write-protected.\n",
					repo_fn_local_str
				);
				goto end;
			}
		} else {
			log_printf("Already got a repository named '%s', ignoring...\n", id);
		}
	} else if(json_is_object(repo_js)) {
		log_printf("Repository file does not specify an ID!\n");
	}

	ret = RepoDiscoverNeighbors(repo_js, id_cache, url_cache);
end:
	json_decref(repo_fn_local);
	SAFE_FREE(repo_buffer);
	json_decref(repo_js);
	json_decref(in_mirrors);
	json_decref(url_cache);
	json_decref(id_cache);
	return ret;
}

int RepoDiscoverAtURL(const char *start_url, json_t *id_cache, json_t *url_cache)
{
	json_t *in_mirrors = ServerBuild(start_url);
	int ret = RepoDiscoverAtServers(in_mirrors, id_cache, url_cache);
	json_decref(in_mirrors);
	return ret;
}

int RepoDiscoverFromLocal(json_t *id_cache, json_t *url_cache)
{
	int ret = 0;
	HANDLE hFind = NULL;
	json_t *repo_js;
	while(repo_js = RepoLocalNext(&hFind)) {
		json_t *servers = json_object_get(repo_js, "servers");
		ret = RepoDiscoverAtServers(servers, id_cache, url_cache);
		if(!ret) {
			ret = RepoDiscoverNeighbors(repo_js, id_cache, url_cache);
		}
		json_decref(repo_js);
	}
	return ret;
}
