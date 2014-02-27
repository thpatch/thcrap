/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Import Address Table detour calls for the win32_utf8 functions.
  */

#include "thcrap.h"

void win32_detour(void)
{
	detour_cache_add("kernel32.dll", 20,
		"CreateDirectoryA", CreateDirectoryU,
		"CreateFileA", CreateFileU,
		"CreateProcessA", CreateProcessU,
		"FindFirstFileA", FindFirstFileU,
		"FindNextFileA", FindNextFileU,
		"FormatMessageA", FormatMessageU,
		"GetCurrentDirectoryA", GetCurrentDirectoryU,
		"GetEnvironmentVariableA", GetEnvironmentVariableU,
		"GetFileAttributesA", GetFileAttributesU,
		"GetFileAttributesExA", GetFileAttributesExU,
		"GetModuleFileNameA", GetModuleFileNameU,
		"GetPrivateProfileIntA", GetPrivateProfileIntU,
		"GetStartupInfoA", GetStartupInfoU,
		"GetTempPathA", GetTempPathU,
		"IsDBCSLeadByte", IsDBCSLeadByteFB,
		"LoadLibraryA", LoadLibraryU,
		"MoveFileA", MoveFileU,
		"MoveFileExA", MoveFileExU,
		"MoveFileWithProgressA", MoveFileWithProgressU,
		"SetCurrentDirectoryA", SetCurrentDirectoryU
	);

	detour_cache_add("gdi32.dll", 3,
		"CreateFontA", CreateFontU,
		"GetTextExtentPoint32A", GetTextExtentPoint32U,
		"TextOutA", TextOutU
	);

	detour_cache_add("shlwapi.dll", 3,
		"PathMatchSpecA", PathMatchSpecU,
		"PathRemoveFileSpecA", PathRemoveFileSpecU,
		"PathFileExistsA", PathFileExistsU
	);

	detour_cache_add("user32.dll", 15,
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
		"SetDlgItemTextA", SetDlgItemTextU,
		"SetWindowLongA", SetWindowLongW,
		"SetWindowLongPtrA", SetWindowLongPtrW,
		"SetWindowTextA", SetWindowTextU
	);
}
