/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Plug-in and module handling
  */

#include "thcrap.h"

static json_t *funcs = NULL;
static json_t *mod_funcs = NULL;
static json_t *plugins = NULL;

void* func_get(const char *name)
{
	return (void*)json_object_get_hex(funcs, name);
}

int plugin_init(HMODULE hMod)
{
	json_t *funcs_new = json_object();
	int ret = GetExportedFunctions(funcs_new, hMod);
	if(!funcs) {
		funcs = json_object();
	}
	if(!mod_funcs) {
		mod_funcs = json_object();
	}
	if(!ret) {
		json_t *mod_funcs_new = mod_func_build(funcs_new);
		const char *key;
		json_t *val;
		mod_func_run(mod_funcs_new, "init", NULL);
		mod_func_run(mod_funcs_new, "detour", NULL);
		json_object_foreach(mod_funcs_new, key, val) {
			json_t *funcs_old = json_object_get_create(mod_funcs, key, JSON_ARRAY);
			json_array_extend(funcs_old, val);
		}
		json_decref(mod_funcs_new);
	}
	json_object_merge(funcs, funcs_new);
	json_decref(funcs_new);
	return ret;
}

int plugins_load(void)
{
	BOOL ret = 0;
	WIN32_FIND_DATAA w32fd;
	HANDLE hFind = FindFirstFile("*.dll", &w32fd);
	if(hFind == INVALID_HANDLE_VALUE) {
		return 1;
	}
	if(!json_is_object(plugins)) {
		plugins = json_object();
	}
	while(!ret) {
		HINSTANCE plugin = LoadLibrary(w32fd.cFileName);
		if(plugin) {
			FARPROC func = GetProcAddress(plugin, "thcrap_plugin_init");
			if(func && !func()) {
				log_printf("\t%s\n", w32fd.cFileName);
				plugin_init(plugin);
				json_object_set_new(plugins, w32fd.cFileName, json_integer((size_t)plugin));
			} else {
				FreeLibrary(plugin);
			}
		}
		ret = W32_ERR_WRAP(FindNextFile(hFind, &w32fd));
	}
	FindClose(hFind);
	return 0;
}

int plugins_close(void)
{
	const char *key;
	json_t *val;

	funcs = json_decref_safe(funcs);
	mod_funcs = json_decref_safe(mod_funcs);

	log_printf("Removing plug-ins...\n");
	json_object_foreach(plugins, key, val) {
		HINSTANCE hInst = (HINSTANCE)json_integer_value(val);
		if(hInst) {
			FreeLibrary(hInst);
		}
	}
	plugins = json_decref_safe(plugins);
	return 0;
}

json_t* mod_func_build(json_t *funcs)
{
	json_t *ret = NULL;
	const char *infix = "_mod_";
	size_t infix_len = strlen(infix);
	const char *key;
	json_t *val;
	json_object_foreach(funcs, key, val) {
		const char *p = strstr(key, infix);
		if(p) {
			json_t *arr = NULL;
			p += infix_len;
			if(!ret) {
				ret = json_object();
			}
			arr = json_object_get_create(ret, p, JSON_ARRAY);
			json_array_append(arr, val);
		}
	}
	return ret;
}

void mod_func_run(json_t *mod_funcs, const char *pattern, void *param)
{
	json_t *func_array = json_object_get(mod_funcs, pattern);
	size_t i;
	json_t *val;
	json_array_foreach(func_array, i, val) {
		mod_call_type func = (mod_call_type)json_integer_value(val);
		if(func) {
			func(param);
		}
	}
}

void mod_func_run_all(const char *pattern, void *param)
{
	mod_func_run(mod_funcs, pattern, param);
}
