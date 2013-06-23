/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * Initialization stubs.
  */

#include "win32_utf8.h"

UINT fallback_codepage = CP_ACP;

void w32u8_set_fallback_codepage(UINT codepage)
{
	fallback_codepage = codepage;
}

void InitDll(HMODULE hMod)
{
	kernel32_init(hMod);
}

void ExitDll(HMODULE hMod)
{
	kernel32_exit();
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
