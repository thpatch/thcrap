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
  * To support Steam integration a patch needs to provide a "steam_appid"
  *	in it's GAMENAME.VERSION.js, GAMENAME.js or global.js file
  */

#include "thcrap.h"

#ifdef _AMD64_
const char *STEAM_API_DLL_FN = "steam_api64.dll";
#else
const char *STEAM_API_DLL_FN = "steam_api.dll";
#endif

typedef const char* TH_CDECL steam_appid_func_t(void);

// Steam API
// ---------
HMODULE hSteamAPI;
HKEY hSteamProcess;

typedef bool TH_CDECL DLL_FUNC_TYPE(SteamAPI, Init)();
typedef bool TH_CDECL DLL_FUNC_TYPE(SteamAPI, Shutdown)();

DLL_FUNC_DEF(SteamAPI, Init);
DLL_FUNC_DEF(SteamAPI, Shutdown);

#define STEAM_GET_PROC_ADDRESS(func) \
	SteamAPI_##func = (SteamAPI_##func##_t*)GetProcAddress(hSteamAPI, "SteamAPI_"#func);
// ---------

extern "C" TH_EXPORT void steam_mod_post_init(void)
{
	std::string appid = "";

	const json_t *appid_obj = json_object_get(runconfig_json_get(), "steam_appid");
	if (appid_obj) {
		switch (json_typeof(appid_obj)) {
		case JSON_STRING:
			appid = json_string_value(appid_obj);
			break;
		case JSON_INTEGER:
			appid = std::to_string(json_integer_value(appid_obj));
			break;
		case JSON_REAL:
			appid = std::to_string(json_real_value(appid_obj));
			break;
		}
	}

	// Got AppID?
	if (appid == "") {
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
	const char *thcrap_dir = runconfig_thcrap_dir_get();
	

	size_t dll_fn_len = strlen(thcrap_dir) + strlen("\\bin\\") + strlen(STEAM_API_DLL_FN) + 1;
	VLA(char, dll_fn, dll_fn_len);
	defer(VLA_FREE(dll_fn));
	strcpy(dll_fn, thcrap_dir);
	strcat(dll_fn, "\\bin\\");
	strcat(dll_fn, STEAM_API_DLL_FN);

	hSteamAPI = LoadLibraryExU(dll_fn, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
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
	
	SetEnvironmentVariableU("SteamAppId", appid.c_str());
	if(!SteamAPI_Init()) {
		// TODO: Figure out why?
		log_printf("[Steam] Initialization for AppID %s failed\n", appid.c_str());
		return;
	}

	log_printf("[Steam] Initialized for AppID %s\n", appid.c_str());
}

extern "C" TH_EXPORT void steam_mod_exit(void)
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
