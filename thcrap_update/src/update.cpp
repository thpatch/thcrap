/**
  * Touhou Community Reliant Automatic Patcher
  * Update plugin
  *
  * ----
  *
  * Main updating functionality.
  */

#include <thcrap.h>
#include <unordered_map>
#include <MMSystem.h>
#include <WinInet.h>
#include <zlib.h>
#include "update.h"

HINTERNET hHTTP = NULL;
SRWLOCK inet_srwlock = {SRWLOCK_INIT};
double perffreq;

typedef struct {
	BYTE *file_buffer;
	DWORD file_size;

	// Absolute timestamps.
	LONGLONG time_start;
	LONGLONG time_ping;
	LONGLONG time_end;
} download_context_t;

int http_init(void)
{
	LARGE_INTEGER pf;
	DWORD ignore = 1;

	// DWORD timeout = 500;
	const char *project_name = PROJECT_NAME();
	size_t agent_len = strlen(project_name) + strlen(" (--) " ) + 16 + 1;
	VLA(char, agent, agent_len);
	sprintf(
		agent, "%s (%s)", project_name, PROJECT_VERSION_STRING()
	);

	QueryPerformanceFrequency(&pf);
	perffreq = (double)pf.QuadPart;

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

get_result_t http_get(download_context_t *ctx, const char *url)
{
	get_result_t get_ret = GET_INVALID_PARAMETER;
	DWORD byte_ret = sizeof(DWORD);
	DWORD http_stat = 0;
	DWORD file_size_local = 0;
	HINTERNET hFile = NULL;

	if(!hHTTP || !url) {
		return GET_INVALID_PARAMETER;
	}
	AcquireSRWLockShared(&inet_srwlock);

	QueryPerformanceCounter((LARGE_INTEGER *)&ctx->time_start);
	hFile = InternetOpenUrl(hHTTP, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
	QueryPerformanceCounter((LARGE_INTEGER *)&ctx->time_ping);
	if(!hFile) {
		DWORD inet_ret = GetLastError();
		switch(inet_ret) {
		case ERROR_INTERNET_NAME_NOT_RESOLVED:
			log_printf("Could not resolve hostname\n", inet_ret);
			break;
		default:
			log_printf("WinInet error %d\n", inet_ret);
			break;
		}
		get_ret = GET_SERVER_ERROR;
		goto end;
	}

	HttpQueryInfo(hFile, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE,
		&http_stat, &byte_ret, 0
	);
	log_printf("%d ", http_stat);
	if(http_stat != 200) {
		get_ret = GET_NOT_AVAILABLE;
		goto end;
	}

	HttpQueryInfo(hFile, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_CONTENT_LENGTH,
		&ctx->file_size, &byte_ret, 0
	);
	ctx->file_buffer = (BYTE*)malloc(ctx->file_size);
	if(ctx->file_buffer) {
		BYTE *p = ctx->file_buffer;
		DWORD read_size = ctx->file_size;
		while(read_size) {
			if(InternetReadFile(hFile, p, ctx->file_size, &byte_ret)) {
				read_size -= byte_ret;
				p += byte_ret;
			} else {
				SAFE_FREE(ctx->file_buffer);
				log_printf("\nReading error #%d! ", GetLastError());
				get_ret = GET_SERVER_ERROR;
				goto end;
			}
		}
		get_ret = GET_OK;
	} else {
		get_ret = GET_OUT_OF_MEMORY;
	}
end:
	QueryPerformanceCounter((LARGE_INTEGER *)&ctx->time_end);

	InternetCloseHandle(hFile);
	ReleaseSRWLockShared(&inet_srwlock);
	return get_ret;
}

void http_exit(void)
{
	if(hHTTP) {
		AcquireSRWLockExclusive(&inet_srwlock);
		InternetCloseHandle(hHTTP);
		hHTTP = NULL;
		ReleaseSRWLockExclusive(&inet_srwlock);
	}
}

/// Internal C++ server connection handling
/// ---------------------------------------
void* server_t::download(
	DWORD *file_size, get_result_t *ret, const char *fn, const DWORD *exp_crc
)
{
	assert(file_size);

	get_result_t temp_ret;
	download_context_t ctx;
	URL_COMPONENTSA uc = {0};
	auto server_len = strlen(this->url) + 1;
	// * 3 because characters may be URL-encoded
	DWORD url_len = server_len + (strlen(fn) * 3) + 1;
	VLA(char, server_host, server_len);
	VLA(char, url, url_len);

	if(!ret) {
		ret = &temp_ret;
	}

	InternetCombineUrl(this->url, fn, url, &url_len, 0);

	uc.dwStructSize = sizeof(uc);
	uc.lpszHostName = server_host;
	uc.dwHostNameLength = server_len;

	InternetCrackUrl(this->url, 0, 0, &uc);

	log_printf("%s (%s)... ", fn, uc.lpszHostName);

	*ret = http_get(&ctx, url);
	*file_size = ctx.file_size;

	VLA_FREE(url);
	VLA_FREE(server_host);

	auto fail = [this, &ctx] (const char *reason) {
		if(reason) {
			log_printf("%s\n", reason);
		}
		this->disable();
		SAFE_FREE(ctx.file_buffer);
		return nullptr;
	};

	// Let's assume that all servers are sane, so rather than drawing some
	// sort of line and saying "well, on *these* errors we'll give it
	// another chance, and on *these* we don't", we'll just disable a
	// server on any error.
	switch(*ret) {
	case GET_INVALID_PARAMETER:
		// Should never really happen, but if it does, we're probably
		// offline anyway.
		return fail(NULL);
	case GET_OUT_OF_MEMORY:
		return fail("Out of memory");
	case GET_SERVER_ERROR:
		// A more detailed error was already printed by the get function.
		return fail(NULL);
	case GET_NOT_AVAILABLE:
		// We used to not disable any server in this case, but think about
		// a situation in which multiple, or even most of the files on a
		// server can't be found. We'd just be wasting time waiting for
		// each of those 404s. Maybe it might be worth it once downloads
		// are faster, but for now, it certainly isn't.
		return fail("Not Available");
	}
	assert(*ret == GET_OK);
	if(*file_size == 0) {
		// This might be a legit non-failure case one day, but until then...
		return fail("0-byte file!");
	}

	// There's not much point in putting a mutex on the time field, but
	// we should at least use a temporary variable here to ensure the
	// correct time being printed.
	auto diff_ping = ctx.time_ping - ctx.time_start;
	auto diff_transfer = ctx.time_end - ctx.time_ping;
	double diff_ping_ms = (diff_ping / perffreq) * 1000.0;
	double diff_transfer_ms = (diff_transfer / perffreq) * 1000.0;
	log_printf(
		"(%d b, %.1f + %.1f ms)\n",
		*file_size, diff_ping_ms, diff_transfer_ms
	);
	this->ping = diff_ping;

	if(exp_crc) {
		auto crc = crc32(0, ctx.file_buffer, ctx.file_size);
		if(*exp_crc != crc) {
			return fail("CRC32 mismatch!");
		}
	}
	return ctx.file_buffer;
}

int servers_t::get_first() const
{
	LONGLONG last_time = LLONG_MAX;
	size_t i = 0;
	int fastest = -1;
	int tryout = -1;

	// Any verification is performed in servers::download().
	if(this->size() == 0) {
		return 0;
	}

	// Get fastest server from previous data
	for(i = 0; i < this->size(); i++) {
		const auto &server = (*this)[i];
		if(server.visited() && server.ping < last_time) {
			last_time = server.ping;
			fastest = i;
		} else if(server.unused()) {
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

int servers_t::num_active() const
{
	int ret = 0;
	for(const auto& i : (*this)) {
		if(i.active()) {
			ret++;
		}
	}
	return ret;
}

void* servers_t::download(DWORD *file_size, const char *fn, const DWORD *exp_crc)
{
	assert(file_size);
	int i;

	int servers_first = this->get_first();
	auto servers_total = this->size();
	auto servers_left = servers_total;

	if(!fn || servers_first < 0) {
		return NULL;
	}
	for(i = servers_first; servers_left; i = (i + 1) % servers_total) {
		get_result_t get_ret;
		server_t &server = (*this)[i];

		servers_left--;
		if(!server.active()) {
			continue;
		} else if(i != servers_first) {
			log_printf("Retrying on next server...\n");
		}

		auto file_buffer = server.download(file_size, &get_ret, fn, exp_crc);

		if(get_ret <= GET_INVALID_PARAMETER) {
			return NULL;
		} else if(file_buffer) {
			return file_buffer;
		}
	}
	return NULL;
}

void servers_t::from(const json_t *servers)
{
	auto servers_len = json_array_size(servers);
	this->resize(servers_len);
	for(size_t i = 0; i < servers_len; i++) {
		json_t *val = json_array_get(servers, i);
		assert(json_is_string(val));
		(*this)[i].url = json_string_value(val);
		(*this)[i].ping = 0;
	}
}

// Needs to be a global rather than a function-local static variable to
// guarantee that it's initialized correctly. Otherwise, operator[] would
// throw a "vector subscript out of range" exception if more than one thread
// called servers_cache() at roughly the same time.
// And yes, that lock is necessary.
SRWLOCK cache_srwlock = {SRWLOCK_INIT};
std::unordered_map<const json_t *, servers_t> patch_servers;

servers_t& servers_cache(const json_t *servers)
{
	AcquireSRWLockExclusive(&cache_srwlock);
	servers_t &srvs = patch_servers[servers];
	ReleaseSRWLockExclusive(&cache_srwlock);
	if(srvs.size() == 0) {
		srvs.from(servers);
	}
	return srvs;
}
/// ---------------------------------------

void* ServerDownloadFile(
	json_t *servers, const char *fn, DWORD *file_size, const DWORD *exp_crc
)
{
	return servers_cache(servers).download(file_size, fn, exp_crc);
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

int update_filter_global(const char *fn, json_t *null)
{
	return strchr(fn, '/') == NULL;
}

int update_filter_games(const char *fn, json_t *games)
{
	STRLEN_DEC(fn);
	size_t i = 0;
	json_t *val;
	json_flex_array_foreach(games, i, val) {
		// We will need to match "th14", but not "th143".
		size_t val_len = json_string_length(val);
		if(
			fn_len > val_len
			&& !strnicmp(fn, json_string_value(val), val_len)
			&& fn[val_len] == '/'
		) {
			return 1;
		}
	}
	return update_filter_global(fn, NULL);
}

json_t* patch_bootstrap(const json_t *sel, json_t *repo_servers)
{
	const char *main_fn = "patch.js";
	void *patch_js_buffer;
	DWORD patch_js_size;
	json_t *patch_info = patch_build(sel);
	const json_t *patch_id = json_array_get(sel, 1);
	size_t patch_len = json_string_length(patch_id) + 1;

	size_t remote_patch_fn_len = patch_len + 1 + strlen(main_fn) + 1;
	VLA(char, remote_patch_fn, remote_patch_fn_len);
	sprintf(remote_patch_fn, "%s/%s", json_string_value(patch_id), main_fn);

	patch_js_buffer = ServerDownloadFile(repo_servers, remote_patch_fn, &patch_js_size, NULL);
	patch_file_store(patch_info, main_fn, patch_js_buffer, patch_js_size);
	// TODO: Nice, friendly error

	VLA_FREE(remote_patch_fn);
	SAFE_FREE(patch_js_buffer);
	return patch_info;
}

int patch_update(json_t *patch_info, update_filter_func_t filter_func, json_t *filter_data)
{
	const char *files_fn = "files.js";

	json_t *local_files = NULL;

	DWORD remote_files_js_size;
	char *remote_files_js_buffer = NULL;

	json_t *remote_files_orig = NULL;
	json_t *remote_files_to_get = NULL;
	json_t *remote_val;

	int ret = 0;
	size_t i = 0;
	size_t file_count = 0;
	size_t file_digits = 0;

	const char *key;
	const char *patch_name = json_object_get_string(patch_info, "id");

	if(!patch_info) {
		return -1;
	}

	// Assuming the repo/patch hierarchy here, but if we ever support
	// repository-less Git patches, they won't be using the files.js
	// protocol anyway.
	if(patch_file_exists(patch_info, "../.git")) {
		if(patch_name) {
			log_printf("(%s is under revision control, not updating.)\n", patch_name);
		}
		ret = 1;
		goto end_update;
	}
	if(json_is_false(json_object_get(patch_info, "update"))) {
		// Updating manually deactivated on this patch
		ret = 1;
		goto end_update;
	}

	auto &servers = servers_cache(json_object_get(patch_info, "servers"));
	if(servers.size() == 0) {
		// No servers for this patch
		ret = 2;
		goto end_update;
	}

	local_files = patch_json_load(patch_info, files_fn, NULL);
	if(!json_is_object(local_files)) {
		local_files = json_object();
	}
	if(patch_name) {
		log_printf("Checking for updates of %s...\n", patch_name);
	}

	remote_files_js_buffer = (char *)servers.download(&remote_files_js_size, files_fn, NULL);
	if(!remote_files_js_buffer) {
		// All servers offline...
		ret = 3;
		goto end_update;
	}

	remote_files_orig = json_loadb_report(remote_files_js_buffer, remote_files_js_size, 0, files_fn);
	if(!json_is_object(remote_files_orig)) {
		// Remote files.js is invalid!
		ret = 4;
		goto end_update;
	}

	// Determine files to download
	remote_files_to_get = json_object();
	json_object_foreach(remote_files_orig, key, remote_val) {
		json_t *local_val = json_object_get(local_files, key);
		if(
			(filter_func ? filter_func(key, filter_data) : 1)
			&& PatchFileRequiresUpdate(patch_info, key, local_val, remote_val)
		) {
			json_object_set(remote_files_to_get, key, remote_val);
		}
	}
	
	file_count = json_object_size(remote_files_to_get);
	if(!file_count) {
		log_printf("Everything up-to-date.\n");
		ret = 0;
		goto end_update;
	}
	file_digits = str_num_digits(file_count);
	log_printf("Need to get %d files.\n", file_count);

	i = 0;
	json_object_foreach(remote_files_to_get, key, remote_val) {
		void *file_buffer;
		DWORD file_size;
		json_t *local_val;

		if(servers.num_active() == 0) {
			ret = 3;
			break;
		}

		log_printf("(%*d/%*d) ", file_digits, ++i, file_digits, file_count);
		local_val = json_object_get(local_files, key);

		// Delete locally unchanged files with a JSON null value in the remote list
		if(json_is_null(remote_val) && json_is_integer(local_val)) {
			file_size = 0;
			file_buffer = patch_file_load(patch_info, key, (size_t*)&file_size);
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
			file_buffer = servers.download(&file_size, key, &remote_crc);
		} else {
			file_buffer = servers.download(&file_size, key, NULL);
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
	json_decref(remote_files_to_get);
	json_decref(remote_files_orig);
	json_decref(local_files);
	return ret;
}

void stack_update(update_filter_func_t filter_func, json_t *filter_data)
{
	json_t *patch_array = json_object_get(runconfig_get(), "patches");
	size_t i;
	json_t *patch_info;
	json_array_foreach(patch_array, i, patch_info) {
		patch_update(patch_info, filter_func, filter_data);
	}
}
