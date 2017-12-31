/**
  * Touhou Community Reliant Automatic Patcher
  * Update plugin
  *
  * ----
  *
  * Plugin setup
  */

#include <thcrap.h>
#include "notify.h"
#include "update.h"

static CRITICAL_SECTION cs_update;
static HMODULE hModule = NULL;

DWORD WINAPI UpdateThread(void *param)
{
	if(!TryEnterCriticalSection(&cs_update)) {
		return 1;
	}
	stack_update(update_filter_games, json_object_get(runconfig_get(), "game"), nullptr, nullptr);
	LeaveCriticalSection(&cs_update);
	return 0;
}

int BP_update_poll(x86_reg_t *regs, json_t *bp_info)
{
	DWORD thread_id;
	CreateThread(NULL, 0, UpdateThread, NULL, 0, &thread_id);
	return 1;
}

int __stdcall thcrap_plugin_init()
{
	if (hModule != NULL) {
		char filename[MAX_PATH];
		GetModuleFileName(hModule, filename, MAX_PATH);
		auto *intended_fn = "\\thcrap_update" DEBUG_OR_RELEASE ".dll";
		if (strcmp(strrchr(filename, '\\'), intended_fn) != 0) {
			return 1;
		}
	}
	update_notify_thcrap();
	update_notify_game();

	BP_update_poll(NULL, NULL);
	return 0;
}

void thcrap_update_exit(void)
{
	http_mod_exit();
}

// Do not call wininet functions in here!
BOOL APIENTRY DllMain(HMODULE hDll, DWORD ulReasonForCall, LPVOID lpReserved)
{
	switch(ulReasonForCall) {
		case DLL_PROCESS_ATTACH:
			InitializeCriticalSection(&cs_update);
			hModule = hDll;
			break;
		case DLL_PROCESS_DETACH:
			DeleteCriticalSection(&cs_update);
			break;
	}
	return TRUE;
}
