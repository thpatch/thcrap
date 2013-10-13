/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Import Address Table detour calls for the win32_utf8 functions.
  */

#include "thcrap.h"

void win32_detour(HMODULE hMod)
{
	// After that Norton incident, I've become a bit scared of AV software,
	// so no patching of CreateProcess and LoadLibrary until we need it
	iat_detour_funcs_var(hMod, "kernel32.dll", 10,
		"CreateDirectoryA", CreateDirectoryU,
		"CreateFileA", CreateFileU,
		"FindFirstFileA", FindFirstFileU,
		"FindNextFileA", FindNextFileU,
		"FormatMessageA", FormatMessageU,
		"GetCurrentDirectoryA", GetCurrentDirectoryU,
		"GetEnvironmentVariableA", GetEnvironmentVariableU,
		"GetModuleFileNameA", GetModuleFileNameU,
		"GetStartupInfoA", GetStartupInfoU,
		"SetCurrentDirectoryA", SetCurrentDirectoryU
	);

	iat_detour_funcs_var(hMod, "gdi32.dll", 3,
		"CreateFontA", CreateFontU,
		"GetTextExtentPoint32A", GetTextExtentPoint32U,
		"TextOutA", TextOutU
	);

	iat_detour_funcs_var(hMod, "shlwapi.dll", 3,
		"PathMatchSpecA", PathMatchSpecU,
		"PathRemoveFileSpecA", PathRemoveFileSpecU,
		"PathFileExistsA", PathFileExistsU
	);

	iat_detour_funcs_var(hMod, "user32.dll", 14,
		"CharNextA", CharNextU,
		"CreateDialogParamA", CreateDialogParamU,
		"CreateWindowExA", CreateWindowExU,
		// Yep, both original functions use the same parameters
		"DefWindowProcA", DefWindowProcW,
		"DialogBoxParamA", DialogBoxParamU,
		"DrawTextA", DrawTextU,
		"GetWindowLongA", GetWindowLongW,
		"GetWindowLongPtrA", GetWindowLongPtrW,
		"MessageBoxA", MessageBoxU,
		"RegisterClassA", RegisterClassU,
		"RegisterClassExA", RegisterClassExU,
		"SetWindowLongA", SetWindowLongW,
		"SetWindowLongPtrA", SetWindowLongPtrW,
		"SetWindowTextA", SetWindowTextU
	);
}
