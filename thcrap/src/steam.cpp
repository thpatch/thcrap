/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Steam integration for games that are available through Steam, but don't
  * come with Steam integration themselves. Requires Valve's `steam_api.dll`,
  * which can be deleted to disable this feature.
  *
  * Initially, this was just meant to show the logged-in Steam user playing
  * the current game, but this also gives us the Steam overlay for free.
  * Doesn't seem to have any drawbacks either.
  *
  * Game-supporting plugins need to implement
  *
  * 	const char* __cdecl steam_appid(void)
  *
  * to return the AppID for the currently running game as a string, or a
  * nullptr if it wasn't released on Steam.
  */

#include "thcrap.h"

#ifdef _AMD64_
const stringref_t STEAM_API_DLL_FN = "steam_api64.dll";
#else
const stringref_t STEAM_API_DLL_FN = "steam_api.dll";
#endif

typedef const char* __cdecl steam_appid_func_t(void);

// Steam API
// ---------
HMODULE hSteamAPI;
HKEY hSteamProcess;

typedef bool __cdecl DLL_FUNC_TYPE(SteamAPI, Init)();
typedef bool __cdecl DLL_FUNC_TYPE(SteamAPI, Shutdown)();

DLL_FUNC_DEF(SteamAPI, Init);
DLL_FUNC_DEF(SteamAPI, Shutdown);

#define STEAM_GET_PROC_ADDRESS(func) \
	SteamAPI_##func = (SteamAPI_##func##_t*)GetProcAddress(hSteamAPI, "SteamAPI_"#func);
// ---------

extern "C" __declspec(dllexport) void steam_mod_post_init(void)
{
	// Got an AppID?
	auto steam_appid_func = (steam_appid_func_t*)func_get("steam_appid");
	if(!steam_appid_func) {
		return;
	}

	auto appid = steam_appid_func();
	if(!appid) {
		return;
	}

	// Got Steam?
	if(RegOpenKeyExA(HKEY_CURRENT_USER,
		"Software\\Valve\\Steam\\ActiveProcess", 0, KEY_READ, &hSteamProcess
	)) {
		log_printf("[Steam] Not installed\n");
		return;
	}

	// Got steam_api.dll?
	stringref_t thcrap_dir = json_object_get(runconfig_get(), "thcrap_dir");

	size_t dll_fn_len = thcrap_dir.len + STEAM_API_DLL_FN.len + 1;
	VLA(char, dll_fn, dll_fn_len);
	defer(VLA_FREE(dll_fn));
	
	char *p = dll_fn;
	p = stringref_copy_advance_dst(p, thcrap_dir);
	p = stringref_copy_advance_dst(p, STEAM_API_DLL_FN);

	hSteamAPI = LoadLibraryU(dll_fn);
	if(!hSteamAPI) {
		log_printf(
			"[Steam] Couldn't load %s (%s), no integration possible\n",
			dll_fn, lasterror_str()
		);
		return;
	}

	STEAM_GET_PROC_ADDRESS(Init);
	STEAM_GET_PROC_ADDRESS(Shutdown);
	if(!SteamAPI_Init || !SteamAPI_Shutdown) {
		log_printf("[Steam] %s corrupt, no integration possible\n", dll_fn);
		return;
	}
	
	SetEnvironmentVariableU("SteamAppId", appid);
	if(!SteamAPI_Init()) {
		// TODO: Figure out why?
		log_printf("[Steam] Initialization for AppID %s failed\n", appid);
		return;
	}

	log_printf("[Steam] Initialized for AppID %s\n", appid);
}

extern "C" __declspec(dllexport) void steam_mod_exit(void)
{
	RegCloseKey(hSteamProcess);
	if(!SteamAPI_Shutdown) {
		return;
	}
	SteamAPI_Shutdown();
	SteamAPI_Init = nullptr;
	SteamAPI_Shutdown = nullptr;
	FreeLibrary(hSteamAPI);
}
