/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Plug-in and module handling
  */

#include "thcrap.h"
#include "plugin.h"

static const char PLUGINS[] = "plugins";

int plugins_load(void)
{
	BOOL ret = 1;
	WIN32_FIND_DATAA w32fd;
	HANDLE hFind = FindFirstFile("*.dll", &w32fd);
	json_t *plugins = json_object_get_create(run_cfg, PLUGINS, JSON_OBJECT);

	if(hFind == INVALID_HANDLE_VALUE) {
		return 1;
	}
	while( (GetLastError() != ERROR_NO_MORE_FILES) && (ret) ) {
		HINSTANCE plugin = LoadLibrary(w32fd.cFileName);
		if(plugin) {
			thcrap_plugin_init_type func =
				(thcrap_plugin_init_type)GetProcAddress(plugin, "thcrap_plugin_init")
			;
			if(func && !func(run_cfg)) {
				log_printf("\t%s\n", w32fd.cFileName);
				json_object_set_new(plugins, w32fd.cFileName, json_integer((size_t)plugin));
			} else {
				FreeLibrary(plugin);
			}
		}
		ret = FindNextFile(hFind, &w32fd);
	}
	FindClose(hFind);
	return 0;
}

int plugins_close(void)
{
	const char *key;
	json_t *val;
	json_t *plugins = json_object_get(run_cfg, PLUGINS);

	log_printf("Removing plug-ins...\n");

	json_object_foreach(plugins, key, val) {
		HINSTANCE hInst = (HINSTANCE)json_integer_value(val);
		if(hInst) {
			FreeLibrary(hInst);
		}
	}
	return 0;
}

void mod_func_run(const char *pattern, void *param)
{
	if(pattern) {
		STRLEN_DEC(pattern);
		size_t suffix_len = strlen("_mod_%s") + pattern_len;
		VLA(char, suffix, suffix_len);
		const char *key;
		json_t *val;
		json_t *funcs = json_object_get(run_cfg, "funcs");
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
