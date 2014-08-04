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
	detour_chain("kernel32.dll", 0,
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

	detour_chain("msvcrt.dll", 0,
		"fopen", fopen_u,
		NULL
	);

	detour_chain("gdi32.dll", 0,
		"CreateFontA", CreateFontU,
		"CreateFontIndirectA", CreateFontIndirectU,
		"CreateFontIndirectExA", CreateFontIndirectExU,
		"EnumFontFamiliesExA", EnumFontFamiliesExU,
		"GetTextExtentPoint32A", GetTextExtentPoint32U,
		"TextOutA", TextOutU,
		NULL
	);

	detour_chain("shell32.dll", 0,
		"SHGetPathFromIDList", SHGetPathFromIDListU,
		"DragQueryFileA", DragQueryFileU,
		NULL
	);

	detour_chain("shlwapi.dll", 0,
		"PathMatchSpecA", PathMatchSpecU,
		"PathRemoveFileSpecA", PathRemoveFileSpecU,
		"PathFileExistsA", PathFileExistsU,
		NULL
	);

	detour_chain("user32.dll", 0,
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
