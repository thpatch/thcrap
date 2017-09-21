/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Wrapper around thcrap_update.dll function, providing a fallback if they are absent.
  */

#pragma once

#include <thcrap_update/src/update.h>

#ifdef __cplusplus
extern "C" {
#endif

void* ServerDownloadFile_wrapper(json_t *servers, const char *fn, DWORD *file_size, const DWORD *exp_crc, file_callback_t callback, void *callback_param);

int update_filter_global_wrapper(const char *fn, json_t *null);
int update_filter_games_wrapper(const char *fn, json_t *games);
int patch_update_wrapper(json_t *patch_info, update_filter_func_t filter_func, json_t *filter_data, patch_update_callback_t callback, void *callback_param);
void stack_update_wrapper(update_filter_func_t filter_func, json_t *filter_data, stack_update_callback_t callback, void *callback_param);
void stack_update_with_UI_progress_wrapper(update_filter_func_t filter_func, json_t *filter_data);

int RepoDiscoverAtURL_wrapper(const char *start_url, json_t *id_cache, json_t *url_cache);
int RepoDiscoverFromLocal_wrapper(json_t *id_cache, json_t *url_cache);

json_t* patch_bootstrap_wrapper(const json_t *sel, json_t *repo_servers);

#ifdef __cplusplus
}
#endif
