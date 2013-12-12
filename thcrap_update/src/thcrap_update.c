/**
  * Touhou Community Reliant Automatic Patcher
  * Update plugin
  *
  * ----
  *
  * Plugin setup
  */

#include <thcrap.h>
#include "update.h"

static CRITICAL_SECTION cs_update;

DWORD WINAPI UpdateThread(void *param)
{
	json_t *run_cfg = (json_t*)param;
	json_t *patch_array = json_object_get(run_cfg, "patches");
	size_t i;
	json_t *patch_info;

	if(!TryEnterCriticalSection(&cs_update)) {
		return 1;
	}
	json_array_foreach(patch_array, i, patch_info) {
		patch_update(patch_info);
	}
	LeaveCriticalSection(&cs_update);
	return 0;
}

int BP_update_poll(x86_reg_t *regs, json_t *bp_info)
{
	DWORD thread_id;
	CreateThread(NULL, 0, UpdateThread, runconfig_get(), 0, &thread_id);
	return 1;
}

int __stdcall thcrap_init_plugin(json_t *run_cfg)
{
	inet_init();

	BP_update_poll(NULL, NULL);
	return 0;
}

int InitDll(HMODULE hDll)
{
	InitializeCriticalSection(&cs_update);
	return 0;
}

void ExitDll(HMODULE hDll)
{
	DeleteCriticalSection(&cs_update);
	inet_exit();
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
