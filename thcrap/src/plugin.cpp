/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Plug-in and module handling
  */

#include "thcrap.h"
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <string_view>

static std::unordered_map<std::string_view, UINT_PTR> funcs = {};
static mod_funcs_t mod_funcs = {};
static json_t *plugins = NULL;

void* func_get(const char *name)
{
	return (void*)funcs[name];
}

void func_add(const char *name, size_t addr) {
	funcs[name] = addr;
}

void func_remove(const char *name) {
	funcs.erase(name);
}

int plugin_init(HMODULE hMod)
{
	exported_func_t *funcs_new;
	int func_count = GetExportedFunctions(&funcs_new, hMod);
	if(func_count > 0) {
		mod_funcs_t *mod_funcs_new = mod_func_build(funcs_new);
		mod_func_run(mod_funcs_new, "init", NULL);
		mod_func_run(mod_funcs_new, "detour", NULL);
		for (mod_func_pair_t pair : *mod_funcs_new) {
			std::string_view key = pair.first;
			std::vector<mod_call_type> arr = pair.second;

			mod_funcs[key].insert(mod_funcs[key].end(), arr.begin(), arr.end());
		}
		for (int i = 0; funcs_new[i].func != 0 && funcs_new[i].name != nullptr; i++) {
			funcs[funcs_new[i].name] = funcs_new[i].func;
		}
		delete mod_funcs_new;
		func_count = 0;
	}	
	return func_count;
}

void plugin_load(const char *dir, const char *fn)
{
	// LoadLibraryEx() with LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
	// requires an absolute path to not fail with GetLastError() == 87.
	STRLEN_DEC(dir);
	STRLEN_DEC(fn);
	VLA(char, fn_abs, dir_len + fn_len);
	defer(VLA_FREE(fn_abs));

	const bool is_debug_plugin = strlen(fn) >= strlen("_d.dll") && strcmp(fn + strlen(fn) - strlen("_d.dll"), "_d.dll") == 0;
#ifdef _DEBUG
	if (!is_debug_plugin) {
		log_printf("[Plugin] %s: release plugin ignored in debug mode (or this dll is not a plugin)\n", fn);
		return;
	}
#else
	if (is_debug_plugin) {
		log_printf("[Plugin] %s: debug plugin ignored in release mode\n", fn);
		return;
	}
#endif

	sprintf(fn_abs, "%s\\%s", dir, fn);

	auto plugin = LoadLibraryExU(fn_abs, nullptr,
		LOAD_WITH_ALTERED_SEARCH_PATH
	);
	if(!plugin) {
		log_printf("[Plugin] Error loading %s: %s\n", fn_abs, lasterror_str());
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
	HANDLE hFind = FindFirstFile("bin\\*.dll", &w32fd);
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

mod_funcs_t* mod_func_build(exported_func_t *funcs)
{
	// This function is not exported
	mod_funcs_t *ret = new mod_funcs_t;
	const char *infix = "_mod_";
	size_t infix_len = strlen(infix);
	for (int i = 0; funcs[i].name != nullptr && funcs[i].func != 0; i++) {
		const char *p = strstr(funcs[i].name, infix);
		if(p) {
			p += infix_len;
			(*ret)[p].push_back((mod_call_type)funcs[i].func);
		}
	}
	return ret;
}

void mod_func_run(mod_funcs_t *mod_funcs, const char *pattern, void *param)
{
	std::vector<mod_call_type> func_array = (*mod_funcs)[pattern];
	for(mod_call_type &func : func_array) {
		if(func) {
			func(param);
		}
	}
}

void mod_func_run_all(const char *pattern, void *param)
{
	mod_func_run(&mod_funcs, pattern, param);
}

void mod_func_remove(const char *pattern, mod_call_type func) {
	std::vector<mod_call_type> func_array = mod_funcs[pattern];
	std::remove_if(func_array.begin(), func_array.end(), [&func](mod_call_type func_in_array) {
		return func_in_array == func;
	});
}
