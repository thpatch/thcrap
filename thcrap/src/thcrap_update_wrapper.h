/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Wrapper around thcrap_update.dll function, providing a fallback if they are absent.
  */

#pragma once

#include <thcrap_update/src/repo.h>
#include <thcrap_update/src/update.h>

#ifdef __cplusplus
extern "C" {
#endif

// Can be used to check if thcrap_update is available.
HMODULE thcrap_update_module(void);

#define DECLARE_WRAPPER(ret, func, ...) typedef ret (*func##_type)(__VA_ARGS__); ret func##_wrapper(__VA_ARGS__);

DECLARE_WRAPPER(void*, ServerDownloadFile, json_t *servers, const char *fn, DWORD *file_size, const DWORD *exp_crc, file_callback_t callback, void *callback_param);

DECLARE_WRAPPER(int, update_filter_global, const char *fn, json_t *null);
DECLARE_WRAPPER(int, update_filter_games, const char *fn, json_t *games);
DECLARE_WRAPPER(int, patch_update, json_t *patch_info, update_filter_func_t filter_func, json_t *filter_data, patch_update_callback_t callback, void *callback_param);
DECLARE_WRAPPER(void, stack_update, update_filter_func_t filter_func, json_t *filter_data, stack_update_callback_t callback, void *callback_param);
DECLARE_WRAPPER(BOOL, loader_update_with_UI, const char *exe_fn, char *args, const char *game_id_fallback);

DECLARE_WRAPPER(int, RepoDiscoverAtURL, const char *start_url, json_t *id_cache, json_t *url_cache, file_write_error_t *fwe_callback);
DECLARE_WRAPPER(int, RepoDiscoverFromLocal, json_t *id_cache, json_t *url_cache, file_write_error_t *fwe_callback);

DECLARE_WRAPPER(json_t*, patch_bootstrap, const json_t *sel, json_t *repo_servers);

DECLARE_WRAPPER(void, thcrap_update_exit);

#undef DECLARE_WRAPPER

#ifdef __cplusplus
}
#endif
