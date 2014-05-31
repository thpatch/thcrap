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
	detour_cache_add("kernel32.dll",
		"CreateDirectoryA", CreateDirectoryU,
		"CreateFileA", CreateFileU,
		"CreateProcessA", CreateProcessU,
		"DeleteFileA", DeleteFileU,
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
		"MultiByteToWideChar", MultiByteToWideCharU,
		"RemoveDirectoryA", RemoveDirectoryU,
		"SetCurrentDirectoryA", SetCurrentDirectoryU,
		"WideCharToMultiByte", WideCharToMultiByteU,
		NULL
	);

	detour_cache_add("msvcrt.dll",
		"fopen", fopen_u,
		NULL
	);

	detour_cache_add("gdi32.dll",
		"CreateFontA", CreateFontU,
		"CreateFontIndirectA", CreateFontIndirectU,
		"CreateFontIndirectExA", CreateFontIndirectExU,
		"EnumFontFamiliesExA", EnumFontFamiliesExU,
		"GetTextExtentPoint32A", GetTextExtentPoint32U,
		"TextOutA", TextOutU,
		NULL
	);

	detour_cache_add("shell32.dll",
		"SHGetPathFromIDList", SHGetPathFromIDListU,
		NULL
	);

	detour_cache_add("shlwapi.dll",
		"PathMatchSpecA", PathMatchSpecU,
		"PathRemoveFileSpecA", PathRemoveFileSpecU,
		"PathFileExistsA", PathFileExistsU,
		NULL
	);

	detour_cache_add("user32.dll",
		"CharNextA", CharNextU,
		"CreateDialogParamA", CreateDialogParamU,
		"CreateWindowExA", CreateWindowExU,
		// Yep, both original functions use the same parameters
		"DefWindowProcA", DefWindowProcW,
		"DialogBoxParamA", DialogBoxParamU,
		"DrawTextA", DrawTextU,
		"GetWindowLongA", GetWindowLongW,
		"GetWindowLongPtrA", GetWindowLongPtrW,
		"InsertMenuItemA", InsertMenuItemU,
		"LoadStringA", LoadStringU,
		"MessageBoxA", MessageBoxU,
		"RegisterClassA", RegisterClassU,
		"RegisterClassExA", RegisterClassExU,
		"SetDlgItemTextA", SetDlgItemTextU,
		"SetWindowLongA", SetWindowLongW,
		"SetWindowLongPtrA", SetWindowLongPtrW,
		"SetWindowTextA", SetWindowTextU,
		NULL
	);
}
