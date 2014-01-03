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
#include <thcrap_update/src/update.h>
#include "repo.h"

json_t* RepoGetLocalFN(const char *id)
{
	const char *repo_fn = "repo.js";
	return json_pack("s++", id, "/", repo_fn);
}

int RepoDiscover(const char *start_url, json_t *id_cache, json_t *url_cache)
{
	int ret = 0;
	const char *repo_fn = "repo.js";
	json_t *in_mirrors = ServerBuild(start_url);
	DWORD repo_size;
	void *repo_buffer = NULL;
	json_t *repo_js = NULL;
	const char *id = NULL;
	json_t *neighbors = NULL;
	size_t i;
	json_t *neighbor;
	json_t *repo_fn_local = NULL;
	const char *repo_fn_local_str;

	url_cache = json_is_object(url_cache) ? json_incref(url_cache) : json_object();
	id_cache = json_is_object(id_cache) ? json_incref(id_cache) : json_object();

	if(!start_url || json_object_get(url_cache, start_url)) {
		goto end;
	} else {
		json_object_set(url_cache, start_url, json_true());
	}
	repo_buffer = ServerDownloadFile(in_mirrors, repo_fn, &repo_size, NULL);
	if(repo_buffer) {
		repo_js = json_loadb_report(repo_buffer, repo_size, 0, repo_fn);
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
			if(ret && !file_write_error(repo_fn_local_str)) {
				goto end;
			}
		} else {
			log_printf("Il existe deja un depot appele '%s', celui-ci est ignore...\n", id);
		}
	} else if(json_is_object(repo_js)) {
		log_printf("Le fichier des depots ne specifie aucun identifiant !\n");
	}

	neighbors = json_object_get(repo_js, "neighbors");
	json_array_foreach(neighbors, i, neighbor) {
		const char *neighbor_str = json_string_value(neighbor);
		// Recursion!
		ret = RepoDiscover(neighbor_str, id_cache, url_cache);
		if(ret) {
			break;
		}
	}
end:
	json_decref(repo_fn_local);
	SAFE_FREE(repo_buffer);
	json_decref(repo_js);
	json_decref(in_mirrors);
	json_decref(url_cache);
	json_decref(id_cache);
	return ret;
}

json_t* RepoLoadLocal(json_t *url_cache)
{
	BOOL find_ret = 1;
	json_t *repo_list;
	WIN32_FIND_DATAA w32fd;
	// Too bad we can't do "*/repo.js" or something similar.
	HANDLE hFind = FindFirstFile("*", &w32fd);

	if(hFind == INVALID_HANDLE_VALUE) {
		return NULL;
	}
	repo_list = json_object();
	while( (GetLastError() != ERROR_NO_MORE_FILES) && (find_ret) ) {
		if(
			(w32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			&& strcmp(w32fd.cFileName, ".")
			&& strcmp(w32fd.cFileName, "..")
		) {
			json_t *repo_local_fn = RepoGetLocalFN(w32fd.cFileName);
			json_t *repo_js = json_load_file_report(json_string_value(repo_local_fn));
			const char *id = json_object_get_string(repo_js, "id");

			json_t *servers = ServerInit(repo_js);
			const json_t *first = json_array_get(servers, 0);
			const char *first_url = json_object_get_string(first, "url");
			RepoDiscover(first_url, NULL, url_cache);

			json_object_set_new(repo_list, id, repo_js);
			json_decref(repo_local_fn);
		}
		find_ret = FindNextFile(hFind, &w32fd);
	}
	FindClose(hFind);
	return repo_list;
}
