/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Plugin setup.
  */

#include <thcrap.h>
#include "thcrap_tsa.h"

int __stdcall thcrap_init_plugin(json_t *run_cfg)
{
	patchhook_register("msg*.dat", patch_msg);
	patchhook_register("*.msg", patch_msg);
	patchhook_register("*.std", patch_std);
	patchhook_register("*.anm", patch_anm);
	spells_init();
	return 0;
}

int InitDll(HMODULE hDll)
{
	return 0;
}

void ExitDll(HMODULE hDll)
{
	spells_exit();
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
