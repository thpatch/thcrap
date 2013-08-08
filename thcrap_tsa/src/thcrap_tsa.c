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
	// th06_msg
	patchhook_register("msg*.dat", patch_msg_dlg); // th06-08
	patchhook_register("p*.msg", patch_msg_dlg); // th09
	patchhook_register("s*.msg", patch_msg_dlg); // lowest common denominator for th10+
	patchhook_register("e*.msg", patch_msg_end); // th10+ endings

	patchhook_register("*.std", patch_std);
	patchhook_register("*.anm", patch_anm);

	// All component initialization functions that require runconfig values
	spells_init();
	return 0;
}

int InitDll(HMODULE hDll)
{
	layout_init(hDll);
	return 0;
}

void ExitDll(HMODULE hDll)
{
	layout_exit();
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
