/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * user32.dll functions.
  */

#pragma once

LPSTR WINAPI CharNextU(
	__in LPSTR lpsz
);
#undef CharNext
#define CharNext CharNextU

HWND WINAPI CreateDialogParamU(
    __in_opt HINSTANCE hInstance,
    __in LPCSTR lpTemplateName,
    __in_opt HWND hWndParent,
    __in_opt DLGPROC lpDialogFunc,
    __in LPARAM dwInitParam
);
#undef CreateDialogParam
#define CreateDialogParam CreateDialogParamU

HWND WINAPI CreateWindowExU(
	__in DWORD dwExStyle,
	__in_opt LPCSTR lpClassName,
	__in_opt LPCSTR lpWindowName,
	__in DWORD dwStyle,
	__in int X,
	__in int Y,
	__in int nWidth,
	__in int nHeight,
	__in_opt HWND hWndParent,
	__in_opt HMENU hMenu,
	__in_opt HINSTANCE hInstance,
	__in_opt LPVOID lpParam
);
#undef CreateWindowEx
#define CreateWindowEx CreateWindowExU

// Yep, both original functions use the same parameters
#undef DefWindowProc
#define DefWindowProc DefWindowProcW

INT_PTR WINAPI DialogBoxParamU(
    __in_opt HINSTANCE hInstance,
    __in LPCSTR lpTemplateName,
    __in_opt HWND hWndParent,
    __in_opt DLGPROC lpDialogFunc,
    __in LPARAM dwInitParam
);
#undef DialogBoxParam
#define DialogBoxParam DialogBoxParamU

int WINAPI MessageBoxU(
	__in_opt HWND hWnd,
	__in_opt LPCSTR lpText,
	__in_opt LPCSTR lpCaption,
	__in UINT uType
);
#undef MessageBox
#define MessageBox MessageBoxU

ATOM WINAPI RegisterClassU(
	__in CONST WNDCLASSA *lpWndClass
);
#undef RegisterClass
#define RegisterClass RegisterClassU

ATOM WINAPI RegisterClassExU(
	__in CONST WNDCLASSEXA *lpWndClass
);
#undef RegisterClassEx
#define RegisterClassEx RegisterClassExU

BOOL WINAPI SetWindowTextU(
	__in HWND hWnd,
	__in_opt LPCSTR lpString
);
#undef SetWindowText
#define SetWindowText SetWindowTextU
