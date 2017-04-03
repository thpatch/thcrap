/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  *
  * ----
  *
  * Repository handling.
  */

#include <thcrap.h>
#include "configure.h"
#include <thcrap/src/thcrap_update_wrapper.h>
#include "repo.h"

json_t* RepoGetLocalFN(const char *id)
{
	const char *repo_fn = "repo.js";
	return json_pack("s++", id, "/", repo_fn);
}

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
	repo_buffer = ServerDownloadFile_wrapper(in_mirrors, repo_fn, &repo_size, NULL);
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
			if(ret && (ret = !file_write_error(repo_fn_local_str))) {
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
	json_t *in_mirrors = ServerBuild_wrapper(start_url);
	int ret = RepoDiscoverAtServers(in_mirrors, id_cache, url_cache);
	json_decref(in_mirrors);
	return ret;
}

json_t* RepoLocalNext(HANDLE *hFind)
{
	json_t *repo_js = NULL;
	WIN32_FIND_DATAA w32fd;
	BOOL find_ret = 0;
	if(*hFind == NULL) {
		// Too bad we can't do "*/repo.js" or something similar.
		*hFind = FindFirstFile("*", &w32fd);
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
			json_t *repo_local_fn = RepoGetLocalFN(w32fd.cFileName);
			repo_js = json_load_file_report(json_string_value(repo_local_fn));
			ServerInit_wrapper(repo_js);
			json_decref(repo_local_fn);
			if(repo_js) {
				return repo_js;
			}
		}
		find_ret = W32_ERR_WRAP(FindNextFile(*hFind, &w32fd));
	};
	FindClose(*hFind);
	return NULL;
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

json_t* RepoLoad(void)
{
	HANDLE hFind = NULL;
	json_t *repo_list = json_object();
	json_t *repo_js;
	while(repo_js = RepoLocalNext(&hFind)) {
		const char *id = json_object_get_string(repo_js, "id");
		json_object_set_new(repo_list, id, repo_js);
	}
	return repo_list;
}
