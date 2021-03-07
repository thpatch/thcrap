#include "thcrap_wrapper.h"

const int BORDER_W = 26;
const int BORDER_H = 13;

static int isCrtInstalled()
{
	// Look in the registry
	HKEY Key;
	size_t pcbData = 4;
	DWORD RtInstalled = 0;
	LSTATUS ret;

	ret = RegOpenKeyW(
		HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\X86",
		&Key
	);

	if (ret == ERROR_SUCCESS) {
		RegQueryValueExW(Key, L"Installed", NULL, NULL, (LPBYTE)&RtInstalled, &pcbData);
		RegCloseKey(Key);
	}
	return RtInstalled;
}

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HFONT hFont = NULL;

	switch (msg)
	{
	case WM_PAINT:
		if (hFont == NULL) {
			NONCLIENTMETRICS ncMetrics;
			OSVERSIONINFO osvi;
			for (unsigned int i = 0; i < sizeof(osvi); i++) ((BYTE*)&osvi)[i] = 0;
			osvi.dwOSVersionInfoSize = sizeof(osvi);
			GetVersionEx(&osvi);
			if (osvi.dwMajorVersion >= 6) {
				ncMetrics.cbSize = sizeof(ncMetrics);
			}
			else {
				// See the remarks in https://docs.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-nonclientmetricsw
				ncMetrics.cbSize = sizeof(ncMetrics) - 4;
			}
			SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncMetrics), &ncMetrics, 0);
			hFont = CreateFontIndirect(&ncMetrics.lfMessageFont);
		}

		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		LPCWSTR text = L"Installing some required files, please wait...";
		HGDIOBJ hOldFont = SelectObject(hdc, hFont);
		TextOut(hdc, BORDER_W, BORDER_H, text, my_wcslen(text));
		SelectObject(hdc, hOldFont);
		EndPaint(hwnd, &ps);
		break;

	case WM_DESTROY:
		if (hFont != NULL) {
			DeleteObject(hFont);
			hFont = NULL;
		}
		break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

static void registerClass()
{
	WNDCLASSEX wc;

	ZeroMemory(&wc, sizeof(wc));
	wc.cbSize = sizeof(wc);
	wc.style = CS_NOCLOSE;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"CRT install popup";
	wc.hIconSm = NULL;

	RegisterClassEx(&wc);
}

static void centerWindow(int w, int h, int *x, int *y)
{
	POINT cursor;
	GetCursorPos(&cursor);

	HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi;
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(monitor, &mi);

	*x = mi.rcWork.left + (mi.rcWork.right  - mi.rcWork.left) / 2 - w / 2;
	*y = mi.rcWork.top  + (mi.rcWork.bottom - mi.rcWork.top) / 2 - h / 2;
}

static HWND createCrtInstallPopup()
{
	HWND hwnd;

	registerClass();

	const int textWidth = 232;
	const int w = textWidth + BORDER_W * 2;
	const int h = 46 + BORDER_H * 2;
	int x;
	int y;
	centerWindow(w, h, &x, &y);

	hwnd = CreateWindow(
		L"CRT install popup",
		L"Touhou Community Reliant Automatic Patcher",
		WS_POPUPWINDOW | WS_CAPTION,
		x, y, w, h,
		NULL, NULL, GetModuleHandle(NULL), NULL);

	ShowWindow(hwnd, SW_SHOWDEFAULT);
	UpdateWindow(hwnd);

	return hwnd;
}

void installCrt(LPWSTR ApplicationPath)
{
	if (isCrtInstalled()) {
		return;
	}

	// Start the VC runtime installer
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;

	for (unsigned int i = 0; i < sizeof(si); i++) ((BYTE*)& si)[i] = 0;
	si.cb = sizeof(si);
	for (unsigned int i = 0; i < sizeof(pi); i++) ((BYTE*)& pi)[i] = 0;

	LPWSTR RtPath = HeapAlloc(GetProcessHeap(), 0, (my_wcslen(ApplicationPath) + my_wcslen(L"\"vc_redist.x86.exe\" /install /quiet /norestart") + 1) * sizeof(wchar_t));
	LPWSTR p = RtPath;
	p = my_strcpy(p, L"\"");
	p = my_strcpy(p, ApplicationPath);
	p = my_strcpy(p, L"vc_redist.x86.exe");
	p = my_strcpy(p, L"\" /install /quiet /norestart");
	BOOL ret = CreateProcess(NULL, RtPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	HeapFree(GetProcessHeap(), 0, RtPath);
	CloseHandle(pi.hThread);

	if (!ret) {
		return;
	}

	// Create a window
	HWND hwnd = createCrtInstallPopup();

	// Wait for the VC runtime installer
	while (1) {
		DWORD waitStatus = MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, INFINITE, QS_ALLEVENTS);
		if (waitStatus == WAIT_OBJECT_0 + 0) { // Process finished
			DestroyWindow(hwnd);
			break;
		}
		else if (WAIT_OBJECT_0 + waitStatus == 1) { // Message on the thread's input queue
			MSG msg;
			int exit = FALSE;
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				if (msg.message == WM_QUIT) {
					PostQuitMessage(0);
					exit = TRUE;
					break;
				}
			}
			if (exit) {
				break;
			}
		}
	}

	// Cleanup
	CloseHandle(pi.hProcess);
}
