/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * JSON data storage for modules.
  */

#include "thcrap.h"

/**
  * This module provides a simple container for other modules to store their
  * custom JSON data. Not only does this remove the need for modules to
  * resolve, keep, and free their data in most cases, it also provides
  * automatic, transparent and thread-safe repatching.
  *
  * Instead of simply storing the contents of every file directly as a value
  * to its file name key, it is wrapped into a JSON array. To repatch a file,
  * jsondata_mod_repatch() then simply inserts the new version to the front
  * of the array. Because jsondata_get() always accesses the first element of
  * this array, it therefore always returns a constant reference to the most
  * current version of a file in memory.
  *
  * This is very important, given that passing constant memory addresses to
  * strings back to the game is one of the main uses of custom JSON data in
  * the first place. Requiring every caller to do reference counting would
  * render this impossible, aside from also complicating the usage of this
  * module.
  *
  * However, this also means that every old version of a file will be kept in
  * memory until jsondata_mod_exit() is called. I see no straightforward way
  * to safely clean up unused references, short of the heap inspection methods
  * used by garbage collectors.
  */

json_t *jsondata = NULL;

template<typename T>
T jsondata_game_func(const char *fn, T (*func)(const char *fn))
{
	char *game_fn = fn_for_game(fn);
	T ret = func(game_fn);
	SAFE_FREE(game_fn);
	return ret;
}

int jsondata_add(const char *fn)
{
	json_t *data_array = NULL;
	json_t *data = NULL;
	// Since the order of the *_mod_init functions is undefined, [jsondata]
	// needs to be created here - otherwise we'll miss some data!
	if(!jsondata) {
		jsondata = json_object();
	}
	data_array = json_object_get_create(jsondata, fn, JSON_ARRAY);
	data = stack_json_resolve(fn, NULL);
	return json_array_insert_new(data_array, 0, data);
}

int jsondata_game_add(const char *fn)
{
	return jsondata_game_func(fn, jsondata_add);
}

json_t* jsondata_get(const char *fn)
{
	return json_array_get(json_object_get(jsondata, fn), 0);
}

json_t* jsondata_game_get(const char *fn)
{
	return jsondata_game_func(fn, jsondata_get);
}

void jsondata_mod_repatch(const json_t *files_changed)
{
	const char *key;
	json_t *val;
	json_object_foreach(jsondata, key, val) {
		if(json_object_get(files_changed, key)) {
			jsondata_add(key);
		}
	}
}

void jsondata_mod_exit(void)
{
	jsondata = json_decref_safe(jsondata);
}
