/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Input detours. Currently providing POV hat → X/Y axis mapping.
  */

#include <thcrap.h>
#include <stdbool.h>
#include <math.h>

// 180° / PI
#define DEG_TO_RAD(x) ((x) * 0.0174532925199432957692369076848f)

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

 typedef struct
{
	uint32_t min;
	uint32_t max;
} range_t;

// joyGetDevCaps() will necessarily be slower
typedef struct
{
	// joyGetPosEx() will return bogus values on joysticks without a POV, so
	// we must check if we even have one.
	bool initialized;
	bool has_pov;
	range_t range_x;
	range_t range_y;
} winmm_joy_caps_t;

winmm_joy_caps_t *joy_info = NULL;

MMRESULT __stdcall my_joyGetPosEx(UINT uJoyID, JOYINFOEX *pji)
{
	if(!pji) {
		return MMSYSERR_INVALPARAM;
	}
	pji->dwFlags |= JOY_RETURNPOV;

	MMRESULT ret_pos = chain_joyGetPosEx(uJoyID, pji);
	if(ret_pos != JOYERR_NOERROR) {
		return ret_pos;
	}

	winmm_joy_caps_t *jc = &joy_info[uJoyID];
	if(!jc->initialized) {
		JOYCAPSW caps;
		MMRESULT ret_caps = joyGetDevCapsW(uJoyID, &caps, sizeof(caps));
		assert(ret_caps == JOYERR_NOERROR);

		jc->initialized = true;
		jc->has_pov = caps.wCaps & JOYCAPS_HASPOV;
		jc->range_x.min = caps.wXmin;
		jc->range_x.max = caps.wXmax;
		jc->range_y.min = caps.wYmin;
		jc->range_y.max = caps.wYmax;
	} else if(!jc->has_pov) {
		return ret_pos;
	}

	if(pji->dwPOV != JOY_POVCENTERED) {
		uint32_t x_center = (jc->range_x.max - jc->range_x.min) / 2;
		uint32_t y_center = (jc->range_y.max - jc->range_y.min) / 2;

		float angle_deg = pji->dwPOV / 100.0f;
		float angle_rad = DEG_TO_RAD(angle_deg);
		// POV values ≠ unit circle angles, so...
		float angle_sin = 1.0f - cosf(angle_rad);
		float angle_cos = sinf(angle_rad) + 1.0f;
		pji->dwXpos = (DWORD)(jc->range_x.min + (angle_cos * x_center));
		pji->dwYpos = (DWORD)(jc->range_y.min + (angle_sin * y_center));
	}
	return ret_pos;
}
/// ------------------

__declspec(dllexport) void input_mod_detour(void)
{
	// This conveniently returns the number of *possible* joysticks, *not* the
	// number of joysticks currently connected.
	UINT num_devs = joyGetNumDevs();
	if(num_devs == 0) {
		return;
	}
	joy_info = (winmm_joy_caps_t*)calloc(sizeof(winmm_joy_caps_t), num_devs);
	if(!joy_info) {
		return;
	}

	detour_chain("winmm.dll", 1,
		"joyGetPosEx", my_joyGetPosEx, &chain_joyGetPosEx,
		NULL
	);
}
