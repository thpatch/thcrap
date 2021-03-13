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

static std::unordered_map<std::string_view, UINT_PTR> funcs = {
	{ "th_malloc", (size_t)&malloc },
	{ "th_calloc", (size_t)&calloc },
	{ "th_realloc", (size_t)&realloc },
	{ "th_free", (size_t)&free },
	{ "th_msize", (size_t)&_msize },
	{ "th_expand", (size_t)&_expand },
	{ "th_aligned_malloc", (size_t)&_aligned_malloc },
	{ "th_aligned_realloc", (size_t)&_aligned_realloc },
	{ "th_aligned_free", (size_t)&_aligned_free },
	{ "th_aligned_msize", (size_t)&_aligned_msize },

	{ "th_memcpy", (size_t)&memcpy },
	{ "th_memmove", (size_t)&memmove },
	{ "th_memcmp", (size_t)&memcmp },
	{ "th_memset", (size_t)&memset },
	{ "th_memccpy", (size_t)&_memccpy },
	{ "th_strdup", (size_t)&strdup },
	{ "th_strndup", (size_t)&strndup },

	{ "th_strcmp", (size_t)&strcmp },
	{ "th_strncmp", (size_t)&strncmp },
	{ "th_stricmp", (size_t)&stricmp },
	{ "th_strnicmp", (size_t)&strnicmp },
	{ "th_strcpy", (size_t)&strcpy },
	{ "th_strncpy", (size_t)&strncpy },
	{ "th_strcat", (size_t)&strcat },
	{ "th_strncat", (size_t)&strncat },
	{ "th_strlen", (size_t)&strlen },
	{ "th_strnlen_s", (size_t)&strnlen_s },

	{ "th_sprintf", (size_t)&sprintf },
	{ "th_snprintf", (size_t)&snprintf },
	{ "th_sscanf", (size_t)&sscanf },

	{ "th_GetLastError", (size_t)&GetLastError },
	{ "th_GetProcAddress", (size_t)&GetProcAddress },
	{ "th_GetModuleHandleA", (size_t)&GetModuleHandleA },
	{ "th_GetModuleHandleW", (size_t)&GetModuleHandleW },
};
static mod_funcs_t mod_funcs = {};
static mod_funcs_t patch_funcs = {};
static json_t *plugins = NULL;

UINT_PTR func_get(const char *name)
{
	auto existing = funcs.find(name);
	if (existing == funcs.end()) {
		return 0;
	} else {
		return existing->second;
	}
}

int func_add(const char *name, size_t addr) {
	// Can this use insert_or_assign somehow?
	auto existing = funcs.find(name);
	if (existing == funcs.end()) {
		funcs[strdup(name)] = addr;
		return 0;
	}
	else {
		log_printf("Overwriting function/codecave %s\n");
		existing->second = addr;
		return 1;
	}
	
}

bool func_remove(const char *name) {
	auto existing = funcs.find(name);
	if (existing != funcs.end()) {
		std::string_view a = existing->first;
		funcs.erase(existing);
		free((void*)a.data());
		return true;
	}
	return false;
}

int patch_func_init(exported_func_t *funcs_new, size_t func_count)
{
	if (func_count > 0) {
		mod_funcs_t *patch_funcs_new = mod_func_build(funcs_new, "_patch_");
		mod_func_run(patch_funcs_new, "init", NULL);
		patch_funcs.merge(*patch_funcs_new);
		delete patch_funcs_new;
		func_count = 0;
	}
	return func_count;
}

int plugin_init(HMODULE hMod)
{
	exported_func_t *funcs_new;
	int func_count = GetExportedFunctions(&funcs_new, hMod);
	if(func_count > 0) {
		mod_funcs_t *mod_funcs_new = mod_func_build(funcs_new, "_mod_");
		mod_func_run(mod_funcs_new, "init", NULL);
		mod_func_run(mod_funcs_new, "detour", NULL);
		mod_funcs.merge(*mod_funcs_new);
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

mod_funcs_t* mod_func_build(exported_func_t *funcs, const char* infix)
{
	// This function is not exported
	mod_funcs_t *ret = new mod_funcs_t;
	const size_t infix_len = strlen(infix);
	for (int i = 0; funcs[i].name != nullptr && funcs[i].func != 0; i++) {
		const char *p = strstr(funcs[i].name, infix);
		if(p) {
			p += infix_len;
			if (p[0] != '\0') {
				(*ret)[p].push_back((mod_call_type)funcs[i].func);
			}
		}
	}
	return ret;
}

void mod_func_run(mod_funcs_t* mod_funcs, const char *pattern, void *param)
{
	std::vector<mod_call_type>& func_array = (*mod_funcs)[pattern];
	for (mod_call_type &func : func_array) {
		if (func) {
			func(param);
		}
	}
}

void mod_func_run_all(const char *pattern, void *param)
{
	mod_func_run(&mod_funcs, pattern, param);
	mod_func_run(&patch_funcs, pattern, param);
}

void patch_func_run_all(const char *pattern, void *param)
{
	mod_func_run(&patch_funcs, pattern, param);
}

void mod_func_remove_from(mod_funcs_t *mod_funcs, const char *pattern, mod_call_type func)
{
	std::vector<mod_call_type> &func_array = (*mod_funcs)[pattern];
	auto elem = std::find_if(func_array.begin(), func_array.end(), [&func](mod_call_type func_in_array) {
		return func_in_array == func;
	});
	func_array.erase(elem);
}

void mod_func_remove(const char *pattern, mod_call_type func)
{
	mod_func_remove_from(&mod_funcs, pattern, func);
}

void patch_func_remove(const char *pattern, mod_call_type func)
{
	mod_func_remove_from(&patch_funcs, pattern, func);
}
