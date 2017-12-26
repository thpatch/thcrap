/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Plug-in and module handling
  */

#include "thcrap.h"
#include <vector>

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

void plugin_load(const char *dir, const char *fn)
{
	STRLEN_DEC(dir);
	STRLEN_DEC(fn);
	VLA(char, fn_abs, dir_len + fn_len);
	defer(VLA_FREE(fn_abs));

	sprintf(fn_abs, "%s/%s", dir, fn);

	HINSTANCE plugin = LoadLibrary(fn_abs);
	if(!plugin) {
		log_printf("[Plugin] Error loading %s: %d\n", fn_abs, GetLastError());
		return;
	}
	FARPROC func = GetProcAddress(plugin, "thcrap_plugin_init");
	if(func && !func()) {
		log_printf("[Plugin] %s: initialized and active\n", fn);
		plugin_init(plugin);
		json_object_set_new(plugins, fn, json_integer((size_t)plugin));
	} else {
		if(func) {
			log_printf("[Plugin] %s: not used for this game\n", fn);
		}
		FreeLibrary(plugin);
	}
}

int plugins_load(const char *dir)
{
	BOOL ret = 0;
	WIN32_FIND_DATAA w32fd;
	HANDLE hFind = FindFirstFile("*.dll", &w32fd);
	if(hFind == INVALID_HANDLE_VALUE) {
		return 1;
	}
	// Apparently, successful self-updates can cause infinite loops?
	// This is safer anyway.
	std::vector<std::string> dlls;
	if(!json_is_object(plugins)) {
		plugins = json_object();
	}
	while(!ret) {
		// Necessary to avoid the nonsensical "Bad Image" message
		// box if you try to LoadLibrary() a 0-byte file.
		if(w32fd.nFileSizeHigh > 0 || w32fd.nFileSizeLow > 0) {
			// Yes, "*.dll" means "*.dll*" in FindFirstFile.
			// https://blogs.msdn.microsoft.com/oldnewthing/20050720-16/?p=34883
			if(!stricmp(PathFindExtensionA(w32fd.cFileName), ".dll")) {
				dlls.push_back(w32fd.cFileName);
			}
		}
		ret = W32_ERR_WRAP(FindNextFile(hFind, &w32fd));
	}
	for(auto dll : dlls) {
		plugin_load(dir, dll.c_str());
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
