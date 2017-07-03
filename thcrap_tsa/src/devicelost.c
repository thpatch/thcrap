/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Device Lost error fixes
  */

#include <thcrap.h>
#include "thcrap_tsa.h"

#define FACILITY_D3D 0x88760000
#define D3D_OK 0
#define D3DERR_DRIVERINTERNALERROR	(FACILITY_D3D|2087)
#define D3DERR_DEVICELOST			(FACILITY_D3D|2152)
#define D3DERR_DEVICENOTRESET		(FACILITY_D3D|2153)

int BP_devicelost(x86_reg_t *regs, json_t *bp_info) {
	// Parameters
	// ----------
	void ***d3dd9 = *(void ****)json_object_get_register(bp_info, regs, "d3dd9");
	void *presParams = (void *)json_object_get_immediate(bp_info, regs, "pres_params");
	// ----------

	// function no. 16 - Reset
	int(__stdcall*d3dd9_Reset)(void***,void*) = (*d3dd9)[16];
	// function no. 3 - TestCooperativeLevel
	int(__stdcall*d3dd9_TestCooperativeLevel)(void***) = (*d3dd9)[3];

	for(;;){	
		switch (d3dd9_TestCooperativeLevel(d3dd9)) {
		case D3DERR_DEVICELOST:
			// wait for a little
			MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
			break;
		case D3DERR_DEVICENOTRESET:
			d3dd9_Reset(d3dd9, presParams);
			break;
		default:
		case D3DERR_DRIVERINTERNALERROR:
			MessageBox(0, "Unable to recover from Device Lost error.", "Error", MB_ICONERROR);
			// panic and return (will probably result in crash)
			// [[fallthrough]]
		case D3D_OK:
			return 0;
		}
	}
}
