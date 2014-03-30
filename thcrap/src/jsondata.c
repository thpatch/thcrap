/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * JSON data storage for modules.
  */

#include "thcrap.h"

json_t *jsondata = NULL;

typedef int (*jsondata_func_t)(const char *fn);

int jsondata_game_func(const char *fn, jsondata_func_t func)
{
	char *game_fn = fn_for_game(fn);
	int ret = func(game_fn);
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
	return (json_t*)jsondata_game_func(fn, (jsondata_func_t)jsondata_get);
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
