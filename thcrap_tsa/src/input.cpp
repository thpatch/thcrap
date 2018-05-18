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
}
