/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Wrapper around thcrap_update.dll function, providing a fallback if they are absent.
  */

#pragma once

#include <thcrap_update/src/repo_discovery.h>
#include <thcrap_update/src/update.h>

#ifdef __cplusplus
extern "C" {
#endif

// Can be used to check if thcrap_update is available.
HMODULE thcrap_update_module(void);

int update_filter_global_wrapper(const char *fn, void*);
int update_filter_games_wrapper(const char *fn, void *games);
void stack_update_wrapper(update_filter_func_t filter_func, void *filter_data, progress_callback_t progress_callback, void *progress_param);
BOOL loader_update_with_UI_wrapper(const char *exe_fn, char *args, const char *game_id_fallback);

repo_t ** RepoDiscover_wrapper(const char *start_url);

patch_t patch_bootstrap_wrapper(const patch_desc_t *sel, const repo_t *repo);

void thcrap_update_exit_wrapper();

#ifdef __cplusplus
}
#endif
