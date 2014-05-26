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
	if(!ret) {
		mod_func_run(funcs_new, "init", NULL);
		mod_func_run(funcs_new, "detour", NULL);
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

void mod_func_run(json_t *funcs, const char *pattern, void *param)
{
	if(json_is_object(funcs) && pattern) {
		STRLEN_DEC(pattern);
		size_t suffix_len = strlen("_mod_%s") + pattern_len;
		VLA(char, suffix, suffix_len);
		const char *key;
		json_t *val;
		suffix_len = snprintf(suffix, suffix_len, "_mod_%s", pattern);
		json_object_foreach(funcs, key, val) {
			size_t key_len = strlen(key);
			const char *key_suffix = key + (key_len - suffix_len);
			if(
				key_len > suffix_len
				&& !memcmp(key_suffix, suffix, suffix_len)
			) {
				mod_call_type func = (mod_call_type)json_integer_value(val);
				if(func) {
					func(param);
				}
			}
		}
		VLA_FREE(suffix);
	}
}

void mod_func_run_all(const char *pattern, void *param)
{
	mod_func_run(funcs, pattern, param);
}
