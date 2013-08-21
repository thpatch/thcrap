/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Import Address Table patch calls for the win32_utf8 functions.
  */

#include "thcrap.h"

void win32_patch(HMODULE hMod)
{
	// After that Norton incident, I've become a bit scared of AV software,
	// so no patching of CreateProcess and LoadLibrary until we need it
	iat_patch_funcs_var(hMod, "kernel32.dll", 9,
		"CreateFileA", CreateFileU,
		"GetModuleFileNameA", GetModuleFileNameU,
		"SetCurrentDirectoryA", SetCurrentDirectoryU,
		"GetCurrentDirectoryA", GetCurrentDirectoryU,
		"GetEnvironmentVariableA", GetEnvironmentVariableU,
		"GetStartupInfoA", GetStartupInfoU,
		"FindFirstFileA", FindFirstFileU,
		"FindNextFileA", FindNextFileU,
		"FormatMessageA", FormatMessageU
	);

	iat_patch_funcs_var(hMod, "gdi32.dll", 3,
		"CreateFontA", CreateFontU,
		"TextOutA", TextOutU,
		"GetTextExtentPoint32A", GetTextExtentPoint32U
	);

	iat_patch_funcs_var(hMod, "user32.dll", 9,
		"CharNextA", CharNextU,
		"CreateDialogParamA", CreateDialogParamU,
		"CreateWindowExA", CreateWindowExU,
		// Yep, both original functions use the same parameters
		"DefWindowProcA", DefWindowProcW,
		"DialogBoxParamA", DialogBoxParamU,
		"MessageBoxA", MessageBoxU,
		"RegisterClassA", RegisterClassU,
		"RegisterClassExA", RegisterClassExU,
		"SetWindowTextA", SetWindowTextU
	);

	iat_patch_funcs_var(hMod, "shlwapi.dll", 3,
		"PathMatchSpecA", PathMatchSpecU,
		"PathRemoveFileSpecA", PathRemoveFileSpecU,
		"PathFileExistsA", PathFileExistsU
	);
}
