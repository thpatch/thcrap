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

#define DECLARE_WRAPPER(ret, func, ...) typedef ret (*func##_type)(__VA_ARGS__); ret func##_wrapper(__VA_ARGS__);

DECLARE_WRAPPER(int, update_filter_global, const char *fn, void*);
DECLARE_WRAPPER(int, update_filter_games, const char *fn, void *games);
DECLARE_WRAPPER(void, stack_update, update_filter_func_t filter_func, void *filter_data, progress_callback_t progress_callback, void *progress_param);
DECLARE_WRAPPER(BOOL, loader_update_with_UI, const char *exe_fn, char *args, const char *game_id_fallback);

DECLARE_WRAPPER(repo_t **, RepoDiscover, const char *start_url);

DECLARE_WRAPPER(patch_t, patch_bootstrap, const patch_desc_t *sel, const repo_t *repo);

DECLARE_WRAPPER(void, thcrap_update_exit);

#undef DECLARE_WRAPPER

#ifdef __cplusplus
}
#endif
