/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Touhou-specific wrappers around Win32 API functions.
  */

#include <thcrap.h>
#include "thcrap_tsa.h"

/// Detour chains
/// -------------
typedef BOOL WINAPI GetWindowRect_type(
	HWND hWnd,
	LPRECT lpRect
);

W32U8_DETOUR_CHAIN_DEF(CreateWindowEx);
DETOUR_CHAIN_DEF(GetWindowRect);
/// -------------

/// Window position hacks
/// ---------------------
/**
  * Recent games call GetWindowRect() before closing to store the current
  * position of the game window in their configuration file. The next time the
  * game is started, this position is used as the parameter for CreateWindowEx().
  *
  * Now, if the game is closed while being minimized, GetWindowRect() reports
  * bogus values (left and top = -32000). With these, CreateWindowEx() will
  * spawn the window at some unreachable "pseudo-minimized" position.
  * (Yeah, maybe it *is* possible to get it back *somehow*, but that way is
  * certainly not user-friendly.)
  *
  * The best solution seems to be replacing GetWindowRect() with
  * GetWindowPlacement(), which always reports the expected window coordinates,
  * even when the window in question is minimized.
  */
BOOL WINAPI tsa_GetWindowRect(
	HWND hWnd,
	LPRECT lpRect
)
{
	BOOL ret;
	WINDOWPLACEMENT wp;
	if(!lpRect) {
		return FALSE;
	}
	ZeroMemory(&wp, sizeof(WINDOWPLACEMENT));
	wp.length = sizeof(WINDOWPLACEMENT);
	ret = GetWindowPlacement(hWnd, &wp);
	memcpy(lpRect, &wp.rcNormalPosition, sizeof(RECT));
	return ret;
}

/**
  * A similar issue like the one with GetWindowRect() occurs when using
  * multiple monitors. When the game window is moved to a secondary monitor,
  * it will also spawn there on new sessions...
  *
  * ... even if the monitor in question is disconnected and the screen space is
  * not available anymore, thus leaving the window in an unreachable position
  * as well. (On Windows 7, you *can* get it back by hovering over its field in
  * the taskbar, right-clicking on the thumbnail and selecting "Move".)
  *
  * The Win32 API provides the current bounding rectangle of the entire screen
  * space through GetSystemMetrics(). We check the coordinates against that,
  * and make sure that the window position comes as close as possible to what
  * is actually requested.
  */

// Ensures the visibility of an imaginary line with length [w1] starting
// at [p1] inside a larger line with length [w2] starting at [p2].
// ... oh well, it's just a helper for tsa_CreateWindowExA().
int coord_clamp(int p1, int w1, int p2, int w2)
{
	int e2 = p2 + w2;
	if((p1 + w1) < p2) {
		p1 = p2;
	} else if(p1 > e2) {
		p1 = e2 - w1;
	}
	return p1;
}

HWND WINAPI tsa_CreateWindowExA(
	DWORD dwExStyle,
	LPCSTR lpClassName,
	LPCSTR lpWindowName,
	DWORD dwStyle,
	int X,
	int Y,
	int nWidth,
	int nHeight,
	HWND hWndParent,
	HMENU hMenu,
	HINSTANCE hInstance,
	LPVOID lpParam
)
{
	HWND ret;
	const char *game_title = runconfig_title_get();
	const char *game_build = runconfig_build_get();

	size_t custom_title_len =
		strlen(game_title) + 1 + strlen(game_build) + 1;
	VLA(char, custom_title, custom_title_len);
	const char *window_title = NULL;

	if(X != CW_USEDEFAULT) {
		X = coord_clamp(
			X, nWidth, GetSystemMetrics(SM_XVIRTUALSCREEN),
			GetSystemMetrics(SM_CXVIRTUALSCREEN)
		);
	}
	if(Y != CW_USEDEFAULT) {
		Y = coord_clamp(
			Y, nHeight, GetSystemMetrics(SM_YVIRTUALSCREEN),
			GetSystemMetrics(SM_CYVIRTUALSCREEN)
		);
	}
	window_title = strings_lookup(lpWindowName, NULL);
	if(window_title == lpWindowName && game_title != nullptr) {
		sprintf(custom_title, "%s %s",
			game_title,
			game_build ? game_build : "");
		window_title = custom_title;
	}
	ret = (HWND)chain_CreateWindowExU(
		dwExStyle, lpClassName, window_title, dwStyle, X, Y,
		nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam
	);
	log_mbox_set_owner(ret);
	VLA_FREE(custom_title);
	return ret;
}
/// ---------------------

void tsa_mod_detour(void)
{
	detour_chain("user32.dll", 1,
		"CreateWindowExA", tsa_CreateWindowExA, &chain_CreateWindowExU,
		"GetWindowRect", tsa_GetWindowRect, &chain_GetWindowRect,
		NULL
	);
}
