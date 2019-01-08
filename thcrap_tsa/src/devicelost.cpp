/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Device Lost error fixes
  */

#include <thcrap.h>
#include <minid3d.h>

typedef HRESULT __stdcall d3dd9_TestCooperativeLevel(void ***);

// Original member function pointers
static int(__stdcall*orig_d3dd9_Reset)(void***, D3DPRESENT_PARAMETERS*) = NULL;

int __stdcall my_d3dd9_Reset(void*** that, D3DPRESENT_PARAMETERS* pPresentationParameters) {
	auto TestCooperativeLevel = (d3dd9_TestCooperativeLevel *)(*that)[3];
	int rv = D3D_OK;
	int coop = TestCooperativeLevel(that);

	if (coop == D3D_OK) {
		// Device is OK, so we're reseting on purpose. (to change the fullscreen mode for example)
		return orig_d3dd9_Reset(that, pPresentationParameters);
	}

	for (;;coop = TestCooperativeLevel(that)) {
		switch (coop) {
		case D3DERR_DEVICELOST:
			// wait for a little
			MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
			break;
		case D3DERR_DEVICENOTRESET:
			rv = orig_d3dd9_Reset(that, pPresentationParameters);
			break;
		default:
		case D3DERR_DRIVERINTERNALERROR:
			MessageBox(0, "Unable to recover from Device Lost error.", "Error", MB_ICONERROR);
			// panic and return (will probably result in crash)
			// [[fallthrough]]
		case D3D_OK:
			return rv;
		}
	}
}

void devicelost_mod_detour(void)
{
	vtable_detour_t my[] = {
		{ 16, my_d3dd9_Reset, (void**)&orig_d3dd9_Reset },
	};
	d3d9_device_detour(my, elementsof(my));
}
