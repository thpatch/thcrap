/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Plug-in handling
  */

#include "thcrap.h"
#include "plugin.h"

static const char PLUGINS[] = "plugins";

int plugins_load(void)
{
	WIN32_FIND_DATAA w32fd;
	HANDLE hFind = FindFirstFile("*.dll", &w32fd);
	json_t *plugins = json_object_get_create(run_cfg, PLUGINS, json_object());

	while( (hFind != INVALID_HANDLE_VALUE) && (GetLastError() != ERROR_NO_MORE_FILES) ) {
		HINSTANCE plugin = LoadLibrary(w32fd.cFileName);
		if(plugin) {
			thcrap_init_plugin_type func;
			func = (thcrap_init_plugin_type)GetProcAddress(plugin, "thcrap_init_plugin");
			if(func && !func(run_cfg)) {
				log_printf("\t%s\n", w32fd.cFileName);
				json_object_set_new(plugins, w32fd.cFileName, json_integer((size_t)plugin));
			} else {
				FreeLibrary(plugin);
			}
		}
		FindNextFile(hFind, &w32fd);
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
