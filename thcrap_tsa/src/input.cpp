/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Input detours. Currently providing POV hat → X/Y axis mapping.
  */

#include <thcrap.h>
#include <math.h>

#define DIRECTINPUT_VERSION 0x800
#include <dinput.h>

/// POV hat → X/Y axis mapping
/// ---------------------------
// 180° / PI
#define DEG_TO_RAD(x) ((x) * 0.0174532925199432957692369076848f)

/*
 * The basic algorithm for converting POV hat values is identical for both
 * WinMM and DirectInput APIs. The only difference is that WinMM expresses
 * its ranges in UINTs ([0; 2³²-1]), while DirectInput uses LONGs
 * ([-2³¹; 2³¹-1]). And well, who knows whether *some* driver manufacturer
 * out there actually uses the entire UINT range for their axis values. So,
 * just to be safe, templates it is.
 */
template <typename T> struct Ranges
{
	T x_min, x_max;
	T y_min, y_max;
};

template <typename T> bool pov_to_xy(T &x, T& y, Ranges<T> &ranges, DWORD pov)
{
	// According to MSDN, some DirectInput drivers report the centered
	// position of the POV indicator as 65,535. This matches both that
	// behavior and JOY_POVCENTERED for WinMM.
	if(LOWORD(pov) == 0xFFFF) {
		return false;
	}
	T x_center = (ranges.x_max - ranges.x_min) / 2;
	T y_center = (ranges.y_max - ranges.y_min) / 2;

	float angle_deg = pov / 100.0f;
	float angle_rad = DEG_TO_RAD(angle_deg);
	// POV values ≠ unit circle angles, so...
	float angle_sin = 1.0f - cosf(angle_rad);
	float angle_cos = sinf(angle_rad) + 1.0f;
	x = (T)(ranges.x_min + (angle_cos * x_center));
	y = (T)(ranges.y_min + (angle_sin * y_center));
	return true;
}
/// ---------------------------

/// WinMM joystick API
/// ------------------
/*
 * Thankfully, the joystick IDs are consistent and always refer to the same
 * model of joystick, even across hot-swaps if you temporarily plug a
 * different model into the same USB port. Even better for our purposes of
 * remapping POV hats to the X/Y axes, the API also treats separate but
 * identical controllers as matching the same ID.
 *
 * The "Preferred device" for older applications that can be set in the
 * control panel will always be assigned to ID 0, even if it was unplugged in
 * the meantime. Setting the Preferred device to (none) will not automatically
 * re-assign another controller to ID 0.
 *
 * Also, IDs are always bound to the same USB port for the lifetime of a
 * process, and controllers won't be recognized at all if they are hot-swapped
 * into a different port than the one they were in before.
 */

typedef MMRESULT __stdcall joyGetPosEx_type(
	UINT uJoyID,
	JOYINFOEX *pji
);

DETOUR_CHAIN_DEF(joyGetPosEx);

// joyGetDevCaps() will necessarily be slower
struct winmm_joy_caps_t
{
	// joyGetPosEx() will return bogus values on joysticks without a POV, so
	// we must check if we even have one.
	bool initialized;
	bool has_pov;
	Ranges<DWORD> range;
};

winmm_joy_caps_t *joy_info = nullptr;

MMRESULT __stdcall my_joyGetPosEx(UINT uJoyID, JOYINFOEX *pji)
{
	if(!pji) {
		return MMSYSERR_INVALPARAM;
	}
	pji->dwFlags |= JOY_RETURNPOV;

	auto ret_pos = chain_joyGetPosEx(uJoyID, pji);
	if(ret_pos != JOYERR_NOERROR) {
		return ret_pos;
	}

	auto *jc = &joy_info[uJoyID];
	if(!jc->initialized) {
		JOYCAPSW caps;
		auto ret_caps = joyGetDevCapsW(uJoyID, &caps, sizeof(caps));
		assert(ret_caps == JOYERR_NOERROR);

		jc->initialized = true;
		jc->has_pov = (caps.wCaps & JOYCAPS_HASPOV) != 0;
		jc->range.x_min = caps.wXmin;
		jc->range.x_max = caps.wXmax;
		jc->range.y_min = caps.wYmin;
		jc->range.y_max = caps.wYmax;
	} else if(!jc->has_pov) {
		return ret_pos;
	}
	pov_to_xy(pji->dwXpos, pji->dwYpos, jc->range, pji->dwPOV);
	return ret_pos;
}
/// ------------------

/// DirectInput API
/// ---------------
#define DIRECTINPUT8CREATE(name) HRESULT __stdcall name( \
	HINSTANCE hinst, \
	DWORD dwVersion, \
	REFIID riidltf, \
	void ****ppvOut, \
	void *punkOuter \
)

#define DI8_CREATEDEVICE(name) HRESULT __stdcall name( \
	void*** that, \
	REFGUID rguid, \
	void ****ppDevice, \
	void *pUnkOuter \
)

#define DID8_GETPROPERTY(name) HRESULT __stdcall name( \
	void*** that, \
	REFGUID rguidProp, \
	DIPROPHEADER *pdiph \
)

#define DID8_SETPROPERTY(name) HRESULT __stdcall name( \
	void*** that, \
	REFGUID rguidProp, \
	const DIPROPHEADER *pdiph \
)

#define DID8_GETDEVICESTATE(name) HRESULT __stdcall name( \
	void*** that, \
	DWORD cbData, \
	void **lpvData \
)

typedef DIRECTINPUT8CREATE(DirectInput8Create_type);
typedef DI8_CREATEDEVICE(DI8_CreateDevice_type);
typedef DID8_GETPROPERTY(DID8_GetProperty_type);
typedef DID8_SETPROPERTY(DID8_SetProperty_type);
typedef DID8_GETDEVICESTATE(DID8_GetDeviceState_type);

DirectInput8Create_type *chain_DirectInput8Create;
DI8_CreateDevice_type *chain_di8_CreateDevice;
DID8_GetProperty_type *chain_did8_GetProperty;
DID8_SetProperty_type *chain_did8_SetProperty;
DID8_GetDeviceState_type *chain_did8_GetDeviceState;

// That disconnect between axes as enumerable "device objects" and the fixed
// DIJOYSTATE structure is pretty weird and confusing. Fingers crossed that
// ZUN will always force all gamepads to the same range. (Which seems likely,
// given the configurable deadzone in custom.exe.)
Ranges<long> di_range;

DID8_SETPROPERTY(my_did8_SetProperty)
{
	auto ret = chain_did8_SetProperty(that, rguidProp, pdiph);
	if(ret != DI_OK || (size_t)&rguidProp != 4 /* DIPROP_RANGE */) {
		return ret;
	}
	auto *prop = (DIPROPRANGE*)pdiph;
	auto &r = di_range;
	if(r.x_min == 0 && r.x_max == 0 && r.y_min == 0 && r.y_max == 0) {
		di_range = { prop->lMin, prop->lMax, prop->lMin, prop->lMax };
	} else if (
		r.x_min != prop->lMin
		|| r.x_max != prop->lMax
		|| r.y_min != prop->lMin
		|| r.y_max != prop->lMax
	) {
		log_mbox(nullptr, MB_OK | MB_ICONEXCLAMATION,
			"Disabling D-pad → axis mapping due to conflicting axis range values.\n"
			"Please report this bug!"
		);
		vtable_detour_t my[] = {
			{ 6, chain_did8_SetProperty, nullptr },
			{ 9, chain_did8_GetDeviceState, nullptr }
		};
		vtable_detour(*that, my, elementsof(my));
	}
	return ret;
}

DID8_GETDEVICESTATE(my_did8_GetDeviceState)
{
	auto ret_state = chain_did8_GetDeviceState(that, cbData, lpvData);
	if(
		ret_state != DI_OK
		|| !(cbData == sizeof(DIJOYSTATE) || cbData == sizeof(DIJOYSTATE2))
	) {
		return ret_state;
	}
	auto *js = (DIJOYSTATE*)lpvData;
	bool ret_map = false;
	for(int i = 0; i < elementsof(js->rgdwPOV) && !ret_map; i++) {
		ret_map = pov_to_xy(js->lX, js->lY, di_range, js->rgdwPOV[i]);
	}
	return ret_state;
}

DI8_CREATEDEVICE(my_di8_CreateDevice)
{
	/*
	 * Little copying > little dependency on dxguid.lib.
	 * Filtering out keyboards is in fact necessary, because:
	 * 1) the Steam overlay assigns different vtables to keyboards and joypads
	 * 2) ZUN initializes the keyboard before any joypads
	 * 3) vtable_detour() then wouldn't detour SetProperty() for joypads,
	 *    because it already did that for keyboards and we assume all
	 *    IDirectInputDevice8 vtables to be equal.
	 *
	 * Also, not every joypad uses GUID_Joystick.
	 */
	const GUID GUID_SysKeyboard = {
		0x6F1D2B61, 0xD5A0, 0x11CF, 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00
	};
	auto ret = chain_di8_CreateDevice(that, rguid, ppDevice, pUnkOuter);
	if(ret != DI_OK || !memcmp(&rguid, &GUID_SysKeyboard, sizeof(GUID))) {
		return ret;
	}
	vtable_detour_t my[] = {
		{ 6, my_did8_SetProperty, (void**)&chain_did8_SetProperty },
		{ 9, my_did8_GetDeviceState, (void**)&chain_did8_GetDeviceState }
	};
	vtable_detour(**ppDevice, my, elementsof(my));
	return ret;
}

DIRECTINPUT8CREATE(my_DirectInput8Create)
{
	if(!chain_DirectInput8Create) {
		return DIERR_INVALIDPARAM;
	}
	auto ret = chain_DirectInput8Create(
		hinst, dwVersion, riidltf, ppvOut, punkOuter
	);
	if(ret != DI_OK) {
		return ret;
	}
	vtable_detour_t my[] = {
		{ 3, my_di8_CreateDevice, (void**)&chain_di8_CreateDevice }
	};
	vtable_detour(**ppvOut, my, elementsof(my));
	return ret;
}
/// ---------------

extern "C" __declspec(dllexport) void input_mod_detour(void)
{
	// This conveniently returns the number of *possible* joysticks, *not* the
	// number of joysticks currently connected.
	auto num_devs = joyGetNumDevs();
	if(num_devs == 0) {
		return;
	}
	joy_info = (winmm_joy_caps_t*)calloc(sizeof(winmm_joy_caps_t), num_devs);
	if(!joy_info) {
		return;
	}

	detour_chain("winmm.dll", 1,
		"joyGetPosEx", my_joyGetPosEx, &chain_joyGetPosEx,
		nullptr
	);

	auto hDInput = GetModuleHandleW(L"dinput8.dll");
	if(hDInput) {
		*(void**)(&chain_DirectInput8Create) = GetProcAddress(hDInput, "DirectInput8Create");
		detour_chain("dinput8.dll", 1,
			"DirectInput8Create", my_DirectInput8Create, &chain_DirectInput8Create,
			nullptr
		);
	}
}
