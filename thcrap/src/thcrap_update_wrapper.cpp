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
#include <thcrap_update/src/loader_update.h>

HMODULE thcrap_update_module(void)
{
	static HMODULE hMod = (HMODULE)-1;

	if (hMod == (HMODULE)-1) {
		bool isWine = GetProcAddress(GetModuleHandleA("KERNEL32"), "wine_get_unix_file_name");
		if (isWine) {
			SetCurrentDirectory("bin");
		}
		hMod = LoadLibraryExU("thcrap_update" DEBUG_OR_RELEASE ".dll", nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
		if (isWine) {
			SetCurrentDirectory("..");
		}
	}
	return hMod;
}

static FARPROC load_thcrap_update_function(const char* func_name)
{
	auto hMod = thcrap_update_module();
	if (hMod) {
		return GetProcAddress(hMod, func_name);
	}
	return NULL;
}

#define CALL_WRAPPED_FUNCTION(func_name, ...) \
static_assert(std::is_same<decltype(func_name), decltype(func_name##_wrapper)>()); \
static FARPROC cached_func = (FARPROC)-1; \
if (cached_func == (FARPROC)-1) { \
	cached_func = load_thcrap_update_function(#func_name); \
} \
if (cached_func) { \
	return ((decltype(&func_name)) cached_func)(__VA_ARGS__); \
}



int update_filter_global_wrapper(const char *fn, void *null)
{
	CALL_WRAPPED_FUNCTION(update_filter_global, fn, null)
	return 0;
}
int update_filter_games_wrapper(const char *fn, void *games)
{
	CALL_WRAPPED_FUNCTION(update_filter_games, fn, games)
	return 0;
}
void stack_update_wrapper(update_filter_func_t filter_func, void *filter_data, progress_callback_t progress_callback, void *progress_param)
{
	CALL_WRAPPED_FUNCTION(stack_update, filter_func, filter_data, progress_callback, progress_param)
}
BOOL loader_update_with_UI_wrapper(const char *exe_fn, char *args, const char *game_id_fallback)
{
	CALL_WRAPPED_FUNCTION(loader_update_with_UI, exe_fn, args, game_id_fallback)
	return thcrap_inject_into_new(exe_fn, args, NULL, NULL);
}

repo_t **RepoDiscover_wrapper(const char *start_url)
{
	CALL_WRAPPED_FUNCTION(RepoDiscover, start_url)
	return RepoLoad();
}

patch_t patch_bootstrap_wrapper(const patch_desc_t *sel, const repo_t *repo)
{
	CALL_WRAPPED_FUNCTION(patch_bootstrap, sel, repo)
	return patch_build(sel);
}

void thcrap_update_exit_wrapper(void)
{
	CALL_WRAPPED_FUNCTION(thcrap_update_exit)
}
