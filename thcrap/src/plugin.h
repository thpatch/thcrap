/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Plug-in and module handling.
  */

#pragma once

/**
  * To be identified as such, every thcrap plugin must export a function named
  *
  * int __stdcall thcrap_plugin_init()
  *
  * which should return 0 on success, and anything else if the plugin should
  * be removed. This function is called directly after the plugin was loaded
  * via LoadLibrary().
  */

// Returns a pointer to a function with the given name in the list of exported
// functions. Basically a GetProcAddress across the engine and all plug-ins.
void* func_get(const char *name);

// Adds a pointer to a function to the list of functions used by func_get
int func_add(const char *name, size_t addr);

/// Module functions
/// ================
/**
  * If the name of a function exported by any thcrap DLL matches the pattern
  * "*_mod_[suffix]", it is automatically executed when calling
  * mod_func_run() with [suffix]. The module hooks currently supported by
  * the thcrap core, with their parameter, include:
  *
  * • "init" (NULL)
  *   Called after a DLL has been loaded.
  *
  * • "detour" (NULL)
  *   Called after a DLL has been loaded. If a module needs to add any new
  *   detours, it should implement this hook, using one or more calls to
  *   detour_cache_add().
  *
  * • "post_init" (NULL)
  *   Called after the initialization of thcrap and all modules is complete.
  *
  * • "repatch" (json_t *files_changed)
  *   Called when the given files have been changed outside the game and need
  *   to be reloaded. [files_changed] is a JSON object with the respective file
  *   names as keys.
  *
  * • "thread_exit" (NULL)
  *   Called whenever a thread in the process exits (DLL_THREAD_DETACH in
  *   DllMain()).
  *
  * • "exit" (NULL)
  *   Called when shutting down the process.
  */

// Module function type.
typedef void (*mod_call_type)(void *param);

// Builds a JSON object mapping the suffixes of all module hook functions
// occurring in [funcs] to an array of pointers to all the functions in
// [funcs] with that suffix:
// {
//	"suffix": [
//		<function pointer>. <function pointer>. ...
//	],
//	...
// }
json_t* mod_func_build(json_t *funcs);

// Runs every module hook function for [suffix] in [mod_funcs]. The execution
// order of the hook functions follows the order their DLLs were originally
// loaded in, but is undefined within the functions of a single DLL.
void mod_func_run(json_t *funcs, const char *suffix, void *param);

// Calls mod_fun_run() with all registered functions from all thcrap DLLs.
void mod_func_run_all(const char *suffix, void *param);
/// ===================

// Initializes a plug-in DLL at [hMod]. This means registering all of its
// exports, and calling its "init" and "detour" module functions.
int plugin_init(HMODULE hMod);

// Loads all thcrap plugins from the given directory.
int plugins_load(const char *dir);

int plugins_close(void);
