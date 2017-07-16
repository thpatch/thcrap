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

// HRESULT values from d3d9.h
#define FACILITY_D3D 0x88760000
#define D3D_OK 0
#define D3DERR_DRIVERINTERNALERROR	(FACILITY_D3D|2087)
#define D3DERR_DEVICELOST			(FACILITY_D3D|2152)
#define D3DERR_DEVICENOTRESET		(FACILITY_D3D|2153)

// vtable indeices
#define D3DD9_TESTCOOPERATIVELEVEL 3
#define D3DD9_RESET 16
#define D3D9_CREATEDEVICE 16

// original memeber function pointers
static int(__stdcall*orig_d3dd9_Reset)(void***, void*) = NULL;
static int(__stdcall*orig_d3d9_CreateDevice)(void***, UINT, int, void*, DWORD, void*, void****) = NULL;
static void***(__stdcall*orig_Direct3DCreate9)(UINT SDKVersion) = NULL;

int __stdcall my_d3dd9_Reset(void*** that, void* pPresentationParameters) {
	int(__stdcall*d3dd9_TestCooperativeLevel)(void***) = (*that)[D3DD9_TESTCOOPERATIVELEVEL];
	int rv = D3D_OK;
	for (;;) {
		switch (d3dd9_TestCooperativeLevel(that)) {
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

int __stdcall my_d3d9_CreateDevice(void*** that, UINT Adapter, int DeviceType, void* hFocusWindow, DWORD BehaviourFlags, void* pPresentationParameters, void**** ppReturnedDeviceInterface) {
	int rv = orig_d3d9_CreateDevice(that, Adapter, DeviceType, hFocusWindow, BehaviourFlags, pPresentationParameters, ppReturnedDeviceInterface);
	if (rv == D3D_OK && ppReturnedDeviceInterface && *ppReturnedDeviceInterface) {
		if (!orig_d3dd9_Reset) {
			void **d3dd9_vtable = **ppReturnedDeviceInterface;
			orig_d3dd9_Reset = d3dd9_vtable[D3DD9_RESET];

			DWORD oldProt;
			VirtualProtect(&d3dd9_vtable[D3DD9_RESET], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
			d3dd9_vtable[D3DD9_RESET] = my_d3dd9_Reset;
			VirtualProtect(&d3dd9_vtable[D3DD9_RESET], sizeof(void*), oldProt, &oldProt);
		}
	}
	return rv;
}

void*** __stdcall my_Direct3DCreate9(UINT SDKVersion) {
	if (!orig_Direct3DCreate9) return NULL;
	void*** rv = orig_Direct3DCreate9(SDKVersion);
	if (rv) {
		if (!orig_d3d9_CreateDevice) {
			void **d3d9_vtable = *rv;
			orig_d3d9_CreateDevice = d3d9_vtable[D3D9_CREATEDEVICE];
			
			DWORD oldProt;
			VirtualProtect(&d3d9_vtable[D3D9_CREATEDEVICE], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
			d3d9_vtable[D3D9_CREATEDEVICE] = my_d3d9_CreateDevice;
			VirtualProtect(&d3d9_vtable[D3D9_CREATEDEVICE], sizeof(void*), oldProt, &oldProt);
		}
	}
	return rv;
}

void devicelost_mod_detour(void)
{
	HMODULE hModule = GetModuleHandle(L"d3d9.dll");
	if (hModule) {
		orig_Direct3DCreate9 = GetProcAddress(hModule, "Direct3DCreate9");
		detour_chain("d3d9.dll", 1,
			"Direct3DCreate9", my_Direct3DCreate9, &orig_Direct3DCreate9,
			NULL
		);
	}
}