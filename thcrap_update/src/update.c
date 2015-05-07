/**
  * Touhou Community Reliant Automatic Patcher
  * Update plugin
  *
  * ----
  *
  * Main updating functionality.
  */

#include <thcrap.h>
#include <MMSystem.h>
#include <WinInet.h>
#include <zlib.h>
#include "update.h"

HINTERNET hHTTP = NULL;
CRITICAL_SECTION cs_http;

int http_init(void)
{
	DWORD ignore = 1;

	// DWORD timeout = 500;
	const char *project_name = PROJECT_NAME();
	size_t agent_len = strlen(project_name) + strlen(" (--) " ) + 16 + 1;
	VLA(char, agent, agent_len);
	sprintf(
		agent, "%s (%s)", project_name, PROJECT_VERSION_STRING()
	);

	InitializeCriticalSection(&cs_http);

	hHTTP = InternetOpenA(agent, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if(!hHTTP) {
		// No internet access...?
		return 1;
	}
	/*
	InternetSetOption(hHTTP, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(DWORD));
	InternetSetOption(hHTTP, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(DWORD));
	InternetSetOption(hHTTP, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(DWORD));
	*/

	// This is necessary when Internet Explorer is set to "work offline"... which
	// will essentially block all wininet HTTP accesses on handles that do not
	// explicitly ignore this setting.
	InternetSetOption(hHTTP, INTERNET_OPTION_IGNORE_OFFLINE, &ignore, sizeof(DWORD));

	VLA_FREE(agent);
	return 0;
}

get_result_t http_get(BYTE **file_buffer, DWORD *file_size, const char *url)
{
	get_result_t get_ret = GET_INVALID_PARAMETER;
	DWORD byte_ret = sizeof(DWORD);
	DWORD http_stat = 0;
	DWORD file_size_local = 0;
	HINTERNET hFile = NULL;

	if(
		!hHTTP || !file_buffer || !file_size || !url
		|| !TryEnterCriticalSection(&cs_http)
	) {
		return GET_INVALID_PARAMETER;
	}

	hFile = InternetOpenUrl(hHTTP, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
	if(!hFile) {
		DWORD inet_ret = GetLastError();
		log_printf("WinInet error %d\n", inet_ret);
		get_ret = GET_SERVER_ERROR;
		goto end;
	}

	HttpQueryInfo(hFile, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &http_stat, &byte_ret, 0);
	log_printf("%d", http_stat);
	if(http_stat != 200) {
		// If the file is missing on one server, it's probably missing on all of them.
		// No reason to disable any server.
		get_ret = GET_NOT_FOUND;
		goto end;
	}

	HttpQueryInfo(hFile, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, file_size, &byte_ret, 0);
	*file_buffer = (BYTE*)malloc(*file_size);
	if(*file_buffer) {
		BYTE *p = *file_buffer;
		DWORD read_size = *file_size;
		while(read_size) {
			BOOL ret = InternetReadFile(hFile, p, *file_size, &byte_ret);
			if(ret) {
				read_size -= byte_ret;
				p += byte_ret;
			} else {
				SAFE_FREE(*file_buffer);
				log_printf("\nReading error #%d!", GetLastError());
				get_ret = GET_SERVER_ERROR;
				goto end;
			}
		}
		get_ret = GET_OK;
	} else {
		get_ret = GET_OUT_OF_MEMORY;
	}
end:
	InternetCloseHandle(hFile);
	LeaveCriticalSection(&cs_http);
	return get_ret;
}

void http_exit(void)
{
	if(hHTTP) {
		EnterCriticalSection(&cs_http);
		InternetCloseHandle(hHTTP);
		hHTTP = NULL;
		LeaveCriticalSection(&cs_http);
		DeleteCriticalSection(&cs_http);
	}
}

json_t* ServerBuild(const char *start_url)
{
	return json_pack("[{sssb}]",
		"url", start_url,
		"time", 1
	);
}

json_t* ServerInit(json_t *patch_js)
{
	json_t *servers = json_object_get(patch_js, "servers");
	json_t *val;
	size_t i;

	json_array_foreach(servers, i, val) {
		json_t *obj = val;
		if(json_is_string(val)) {
			// Convert to object
			obj = json_object();
			json_object_set(obj, "url", val);
			json_array_set_new(servers, i, obj);
		}
		json_object_set(obj, "time", json_true());
	}
	return servers;
}

void ServerNewSession(json_t *servers)
{
	size_t i;
	json_t *server;

	json_array_foreach(servers, i, server) {
		json_object_set_nocheck(server, "time", json_true());
	}
}

const int ServerGetFirst(const json_t *servers)
{
	unsigned int last_time = -1;
	size_t i = 0;
	int fastest = -1;
	int tryout = -1;
	json_t *server;

	// Any verification is performed in ServerDownloadFile().
	if(json_array_size(servers) == 0) {
		return 0;
	}

	// Get fastest server from previous data
	json_array_foreach(servers, i, server) {
		const json_t *server_time = json_object_get(server, "time");
		if(json_is_integer(server_time)) {
			json_int_t cur_time = json_integer_value(server_time);
			if(cur_time < last_time) {
				last_time = (unsigned int)cur_time;
				fastest = i;
			}
		} else if(json_is_true(server_time)) {
			tryout = i;
		}
	}
	if(tryout != -1) {
		return tryout;
	} else if(fastest != -1) {
		return fastest;
	} else {
		// Everything down...
		return -1;
	}
}

size_t ServerGetNumActive(const json_t *servers)
{
	size_t i;
	json_t *server;
	int ret = json_array_size(servers);

	json_array_foreach(servers, i, server) {
		if(json_is_false(json_object_get(server, "time"))) {
			ret--;
		}
	}
	return ret;
}

int ServerDisable(json_t *server)
{
	json_object_set(server, "time", json_false());
	return 0;
}

void* ServerDownloadFile(
	json_t *servers, const char *fn, DWORD *file_size, const DWORD *exp_crc
)
{
	BYTE *file_buffer = NULL;
	int i;

	int servers_first = ServerGetFirst(servers);
	int servers_total = json_array_size(servers);
	// gets decremented in the loop
	int servers_left = servers_total;

	if(!fn || !file_size || servers_first < 0) {
		return NULL;
	}
	*file_size = 0;
	for(i = servers_first; servers_left; i = (i + 1) % servers_total) {
		DWORD time_start;
		DWORD time;
		DWORD crc = 0;
		get_result_t get_ret;
		json_t *server = json_array_get(servers, i);
		json_t *server_time = json_object_get(server, "time");

		if(json_is_false(server_time)) {
			continue;
		}
		servers_left--;

		{
			URL_COMPONENTSA uc = {0};
			const json_t *server_url_obj = json_object_get(server, "url");
			const char *server_url = json_string_value(server_url_obj);
			size_t server_url_len = json_string_length(server_url_obj);
			// * 3 because characters may be URL-encoded
			DWORD url_len = server_url_len + 1 + (strlen(fn) * 3) + 1;
			VLA(char, server_host, server_url_len);
			VLA(char, url, url_len);

			InternetCombineUrl(server_url, fn, url, &url_len, 0);

			uc.dwStructSize = sizeof(uc);
			uc.lpszHostName = server_host;
			uc.dwHostNameLength = server_url_len;

			InternetCrackUrl(server_url, 0, 0, &uc);

			log_printf("%s (%s)... ", fn, uc.lpszHostName);

			time_start = timeGetTime();

			get_ret = http_get(&file_buffer, file_size, url);

			VLA_FREE(url);
			VLA_FREE(server_host);
		}

		switch(get_ret) {
			case GET_OUT_OF_MEMORY:
			case GET_INVALID_PARAMETER:
				goto abort;

			case GET_SERVER_ERROR:
				ServerDisable(server);
				continue;
		}
		if(*file_size == 0) {
			log_printf(" 0-byte file! %s\n", servers_left ? "Trying next server..." : "");
			ServerDisable(server);
			continue;
		}

		time = timeGetTime() - time_start;
		log_printf(" (%d b, %d ms)\n", *file_size, time);
		json_object_set_new(server, "time", json_integer(time));

		crc = crc32(crc, file_buffer, *file_size);
		if(exp_crc && *exp_crc != crc) {
			log_printf("CRC32 mismatch! %s\n", servers_left ? "Trying next server..." : "");
			ServerDisable(server);
			SAFE_FREE(file_buffer);
		} else {
			return file_buffer;
		}
	}
abort:
	SAFE_FREE(file_buffer);
	return NULL;
}

int PatchFileRequiresUpdate(const json_t *patch_info, const char *fn, json_t *local_val, json_t *remote_val)
{
	// Remove if the remote specifies a JSON null,
	// but skip if the file doesn't exit locally
	if(json_is_null(remote_val)) {
		return local_val != NULL && patch_file_exists(patch_info, fn);
	}
	// Update if remote and local JSON values don't match
	if(!json_equal(local_val, remote_val)) {
		return 1;
	}
	// Update if local file doesn't exist
	if(!patch_file_exists(patch_info, fn)) {
		return 1;
	}
	return 0;
}

int patch_update(json_t *patch_info)
{
	const char *files_fn = "files.js";

	json_t *servers = NULL;
	json_t *local_files = NULL;

	DWORD remote_files_js_size;
	BYTE *remote_files_js_buffer = NULL;

	json_t *remote_files = NULL;
	json_t *remote_val;

	int ret = 0;
	size_t i = 0;
	size_t file_count = 0;
	size_t file_digits = 0;

	const char *key;

	if(!patch_info) {
		return -1;
	}

	if(json_is_false(json_object_get(patch_info, "update"))) {
		// Updating deactivated on this patch
		ret = 1;
		goto end_update;
	}

	servers = ServerInit(patch_info);
	if(!json_is_array(servers)) {
		// No servers for this patch
		ret = 2;
		goto end_update;
	}

	local_files = patch_json_load(patch_info, files_fn, NULL);
	if(!json_is_object(local_files)) {
		local_files = json_object();
	}
	{
		const char *patch_name = json_object_get_string(patch_info, "id");
		if(patch_name) {
			log_printf("Checking for updates of %s...\n", patch_name);
		}
	}

	remote_files_js_buffer = ServerDownloadFile(servers, files_fn, &remote_files_js_size, NULL);
	if(!remote_files_js_buffer) {
		// All servers offline...
		ret = 3;
		goto end_update;
	}

	remote_files = json_loadb_report(remote_files_js_buffer, remote_files_js_size, 0, files_fn);
	if(!json_is_object(remote_files)) {
		// Remote files.js is invalid!
		ret = 4;
		goto end_update;
	}

	// Yay for doubled loops... just to get the correct number
	json_object_foreach(remote_files, key, remote_val) {
		json_t *local_val = json_object_get(local_files, key);
		if(PatchFileRequiresUpdate(patch_info, key, local_val, remote_val)) {
			file_count++;
		}
	}
	if(!file_count) {
		log_printf("Everything up-to-date.\n", file_count);
		ret = 0;
		goto end_update;
	}

	file_digits = str_num_digits(file_count);
	log_printf("Need to get %d files.\n", file_count);

	i = 0;
	json_object_foreach(remote_files, key, remote_val) {
		void *file_buffer;
		DWORD file_size;
		json_t *local_val;

		if(!ServerGetNumActive(servers)) {
			ret = 3;
			break;
		}

		local_val = json_object_get(local_files, key);
		if(!PatchFileRequiresUpdate(patch_info, key, local_val, remote_val)) {
			continue;
		}
		i++;

		log_printf("(%*d/%*d) ", file_digits, i, file_digits, file_count);

		// Delete locally unchanged files with a JSON null value in the remote list
		if(json_is_null(remote_val) && json_is_integer(local_val)) {
			file_size = 0;
			file_buffer = patch_file_load(patch_info, key, &file_size);
			if(file_buffer && file_size) {
				DWORD local_crc = crc32(0, (Bytef*)file_buffer, file_size);
				if(local_crc == json_integer_value(local_val)) {
					log_printf("Deleting %s...\n", key);
					if(!patch_file_delete(patch_info, key)) {
						json_object_del(local_files, key);
					}
				} else {
					log_printf("%s (locally changed, skipping deletion)\n", key);
				}
			}
			SAFE_FREE(file_buffer);
		} else if(json_is_integer(remote_val)) {
			DWORD remote_crc = json_integer_value(remote_val);
			file_buffer = ServerDownloadFile(servers, key, &file_size, &remote_crc);
		} else {
			file_buffer = ServerDownloadFile(servers, key, &file_size, NULL);
		}
		if(file_buffer) {
			patch_file_store(patch_info, key, file_buffer, file_size);
			SAFE_FREE(file_buffer);
			json_object_set(local_files, key, remote_val);
		}
		patch_json_store(patch_info, files_fn, local_files);
	}
	if(i == file_count) {
		log_printf("Update completed.\n");
	}

	// I thought 15 minutes about this, and considered it appropriate
end_update:
	if(ret == 3) {
		log_printf("Can't reach any valid server at the moment.\nCancelling update...\n");
	}
	SAFE_FREE(remote_files_js_buffer);
	json_decref(remote_files);
	json_decref(local_files);
	return ret;
}

void stack_update(void)
{
	json_t *patch_array = json_object_get(runconfig_get(), "patches");
	size_t i;
	json_t *patch_info;
	json_array_foreach(patch_array, i, patch_info) {
		patch_update(patch_info);
	}
}
