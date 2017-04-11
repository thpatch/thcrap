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

DWORD WINAPI UpdateThread(void *param)
{
	if(!TryEnterCriticalSection(&cs_update)) {
		return 1;
	}
	stack_update(update_filter_games, json_object_get(runconfig_get(), "game"));
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
	update_notify_thcrap();
	update_notify_game();

	BP_update_poll(NULL, NULL);
	return 0;
}

int InitDll(HMODULE hDll)
{
	http_init();
	InitializeCriticalSection(&cs_update);
	return 0;
}

void ExitDll(HMODULE hDll)
{
	DeleteCriticalSection(&cs_update);
	http_exit();
}

// Yes, this _has_ to be included in every project.
// Visual C++ won't use it when imported from a library
// and just defaults to msvcrt's one in this case.
BOOL APIENTRY DllMain(HMODULE hDll, DWORD ulReasonForCall, LPVOID lpReserved)
{
	switch(ulReasonForCall) {
		case DLL_PROCESS_ATTACH:
			InitDll(hDll);
			break;
		case DLL_PROCESS_DETACH:
			ExitDll(hDll);
			break;
	}
	return TRUE;
}
