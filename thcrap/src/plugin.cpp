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
//static std::unordered_map<std::string_view, HMODULE> plugins;
static std::vector<HMODULE> plugins;

UINT_PTR func_get(const char *name)
{
	auto existing = funcs.find(name);
	if (existing == funcs.end()) {
		return 0;
	} else {
		return existing->second;
	}
}

UINT_PTR func_get_len(const char *name, size_t name_len)
{
	auto existing = funcs.find({ name, name_len });
	if (existing == funcs.end()) {
		return 0;
	}
	else {
		return existing->second;
	}
}

int func_add(const char *name, size_t addr) {
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
	auto& former_func = funcs.extract(name);
	const bool func_removed = !former_func.empty();
	if (func_removed) {
		free((void*)former_func.key().data());
	}
	return func_removed;
}

int patch_func_init(exported_func_t *funcs)
{
	if (funcs) {
		mod_funcs_t patch_funcs_new;
		patch_funcs_new.build(funcs, "_patch_");
		patch_funcs_new.run("init", NULL);
		patch_funcs.merge(patch_funcs_new);
	}
	return 0;
}

int plugin_init(HMODULE hMod)
{
	if(exported_func_t* funcs_new = GetExportedFunctions(hMod)) {
		mod_funcs_t mod_funcs_new;
		mod_funcs_new.build(funcs_new, "_mod_");
		mod_funcs_new.run("init", NULL);
		mod_funcs_new.run("detour", NULL);
		mod_funcs.merge(mod_funcs_new);
		for (int i = 0; funcs_new[i].func != 0 && funcs_new[i].name != nullptr; i++) {
			funcs[funcs_new[i].name] = funcs_new[i].func;
		}
		free(funcs_new);
	}	
	return 0;
}

void plugin_load(const char *const fn_abs, const char *fn)
{
	const size_t fn_len = strlen(fn);
	const size_t dbg_len = strlen("_d.dll");
	const bool is_debug_plugin = fn_len >= dbg_len && strcmp(fn + fn_len - dbg_len, "_d.dll") == 0;
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

	if (HMODULE plugin = LoadLibraryExU(fn_abs, NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) {
		switch (FARPROC func = GetProcAddress(plugin, "thcrap_plugin_init");
				(uintptr_t)func) {
			default:
				if (!func()) {
					log_printf("[Plugin] %s: initialized and active\n", fn);
					plugin_init(plugin);
					plugins.push_back(plugin);
					break;
				}
				log_printf("[Plugin] %s: not used for this game\n", fn);
				[[fallthrough]];
			case NULL:
				FreeLibrary(plugin);
		}
	}
	else {
		log_printf("[Plugin] Error loading %s: %s\n", fn_abs, lasterror_str());
	}
}

int plugins_load(const char *dir)
{
	// Apparently, successful self-updates can cause infinite loops?
	// This is safer anyway.
	std::vector<char*> dlls;

	const size_t dir_len = strlen(dir);
	char* const dll_path = strndup(dir, MAX_PATH + 1);
	strcat(dll_path, "\\*.dll");

	{
		WIN32_FIND_DATAA w32fd;
		HANDLE hFind = FindFirstFile(dll_path, &w32fd);
		if (hFind == INVALID_HANDLE_VALUE) {
			return 1;
		}
		do {
			// Necessary to avoid the nonsensical "Bad Image" message
			// box if you try to LoadLibrary() a 0-byte file.
			if (w32fd.nFileSizeLow | w32fd.nFileSizeHigh) {
				// Yes, "*.dll" means "*.dll*" in FindFirstFile.
				// https://blogs.msdn.microsoft.com/oldnewthing/20050720-16/?p=34883
				if (!stricmp(PathFindExtensionA(w32fd.cFileName), ".dll")) {
					dlls.push_back(strdup(w32fd.cFileName));
				}
			}
		} while (FindNextFile(hFind, &w32fd));
		FindClose(hFind);
	}
	
	// LoadLibraryEx() with LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
	// requires an absolute path to not fail with GetLastError() == 87.
	char *const fn_start = &dll_path[dir_len + 1];
	for (auto dll : dlls) {
		strcpy(fn_start, dll);
		plugin_load(dll_path, fn_start);
		free(dll);
	}
	free(dll_path);
	return 0;
}

int plugins_close(void)
{
	log_printf("Removing plug-ins...\n");
	for (HMODULE plugin_module : plugins) {
		FreeLibrary(plugin_module);
	}
	return 0;
}

inline void mod_funcs_t::build(exported_func_t* funcs, const char* infix) {
	if (funcs && infix) {
		const size_t infix_len = strlen(infix);
		while (funcs->name && funcs->func) {
			if (const char* p = strstr(funcs->name, infix);
				p && *(p += infix_len) != '\0') {
				(*this)[p].push_back((mod_call_type)funcs->func);
			}
			++funcs;
		}
	}
}

inline void mod_funcs_t::run(std::string_view suffix, void* param) {
	std::vector<mod_call_type>& func_array = (*this)[suffix];
	for (mod_call_type func : func_array) {
		func(param);
	}
}

void mod_func_run_all(const char *pattern, void *param)
{
	mod_funcs.run(pattern, param);
	patch_funcs.run(pattern, param);
}

void patch_func_run_all(const char *pattern, void *param)
{
	patch_funcs.run(pattern, param);
}

inline void mod_funcs_t::remove(std::string_view suffix, mod_call_type func)
{
	std::vector<mod_call_type>& func_array = (*this)[suffix];
	auto elem = std::find_if(func_array.begin(), func_array.end(), [&func](mod_call_type func_in_array) {
		return func_in_array == func;
		});
	func_array.erase(elem);
}

void mod_func_remove(const char *pattern, mod_call_type func)
{
	mod_funcs.remove(pattern, func);
}

void patch_func_remove(const char *pattern, mod_call_type func)
{
	patch_funcs.remove(pattern, func);
}
