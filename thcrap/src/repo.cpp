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

json_t* RepoGetLocalFN(const char *id)
{
	const char *repo_fn = "repo.js";
	return json_pack("s+++", "repos/", id, "/", repo_fn);
}

json_t* RepoLocalNext(HANDLE *hFind)
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
			json_t *repo_local_fn = RepoGetLocalFN(w32fd.cFileName);
			repo_js = json_load_file_report(json_string_value(repo_local_fn));
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
