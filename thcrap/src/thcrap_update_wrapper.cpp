/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Wrapper around thcrap_update.dll function, providing a fallback if they are absent.
  */

#include "thcrap.h"
#include "thcrap_update_wrapper.h"
#include <thcrap_update\src\loader_update.h>
#include <thcrap_update\src\repo.h>

static FARPROC load_thcrap_update_function(const char* func_name)
{
	static HMODULE hMod = (HMODULE)-1;

	if (hMod == (HMODULE)-1) {
#ifdef _DEBUG
		hMod = LoadLibrary("thcrap_update_d.dll");
#else
		hMod = LoadLibrary("thcrap_update.dll");
#endif
	}

	if (hMod) {
		return GetProcAddress(hMod, func_name);
	}
	return NULL;
}

#define CALL_WRAPPED_FUNCTION(func_name, ...) \
static FARPROC cached_func = (FARPROC)-1; \
if (cached_func == (FARPROC)-1) { \
	cached_func = load_thcrap_update_function(#func_name); \
} \
if (cached_func) { \
	return ((func_name##_type) cached_func)(__VA_ARGS__); \
}

#define ASSERT_FUNCTION_PROTO(func) static decltype(func) *func##_testpointer = func##_wrapper;



void* ServerDownloadFile_wrapper(json_t *servers, const char *fn, DWORD *file_size, const DWORD *exp_crc, file_callback_t callback, void *callback_param)
{
	CALL_WRAPPED_FUNCTION(ServerDownloadFile, servers, fn, file_size, exp_crc, callback, callback_param)
	return NULL;
}
ASSERT_FUNCTION_PROTO(ServerDownloadFile);

int update_filter_global_wrapper(const char *fn, json_t *null)
{
	CALL_WRAPPED_FUNCTION(update_filter_global, fn, null)
	return 0;
}
ASSERT_FUNCTION_PROTO(update_filter_global);
int update_filter_games_wrapper(const char *fn, json_t *games)
{
	CALL_WRAPPED_FUNCTION(update_filter_games, fn, games)
	return 0;
}
ASSERT_FUNCTION_PROTO(update_filter_games);
int patch_update_wrapper(json_t *patch_info, update_filter_func_t filter_func, json_t *filter_data, patch_update_callback_t callback, void *callback_param)
{
	CALL_WRAPPED_FUNCTION(patch_update, patch_info, filter_func, filter_data, callback, callback_param)
	return 3;
}
ASSERT_FUNCTION_PROTO(patch_update);
void stack_update_wrapper(update_filter_func_t filter_func, json_t *filter_data, stack_update_callback_t callback, void *callback_param)
{
	CALL_WRAPPED_FUNCTION(stack_update, filter_func, filter_data, callback, callback_param)
}
ASSERT_FUNCTION_PROTO(stack_update);
BOOL loader_update_with_UI_wrapper(const char *exe_fn, char *args)
{
	CALL_WRAPPED_FUNCTION(loader_update_with_UI, exe_fn, args)
	return 1;
}
ASSERT_FUNCTION_PROTO(loader_update_with_UI);

int RepoDiscoverAtURL_wrapper(const char *start_url, json_t *id_cache, json_t *url_cache)
{
	CALL_WRAPPED_FUNCTION(RepoDiscoverAtURL, start_url, id_cache, url_cache)
	return 0;
}
ASSERT_FUNCTION_PROTO(RepoDiscoverAtURL);

int RepoDiscoverFromLocal_wrapper(json_t *id_cache, json_t *url_cache)
{
	CALL_WRAPPED_FUNCTION(RepoDiscoverFromLocal, id_cache, url_cache)
	return 0;
}
ASSERT_FUNCTION_PROTO(RepoDiscoverFromLocal);

json_t *patch_bootstrap_wrapper(const json_t *sel, json_t *repo_servers)
{
	CALL_WRAPPED_FUNCTION(patch_bootstrap, sel, repo_servers)
	return patch_build(sel);
}
ASSERT_FUNCTION_PROTO(patch_bootstrap);
