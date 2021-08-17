#include "thcrap_wrapper.h"

const int BORDER_W = 26;
const int BORDER_H = 13;

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HFONT hFont = NULL;
	static LPWSTR install_message = NULL;

	switch (msg)
	{
	case WM_CREATE:
		CREATESTRUCTW* create_params = (CREATESTRUCTW*)lParam;
		install_message = (LPWSTR)create_params->lpCreateParams;
		break;
	case WM_PAINT:
		if (hFont == NULL) {
			NONCLIENTMETRICS ncMetrics;
			OSVERSIONINFO osvi;
			my_memset(&osvi, 0, sizeof(osvi));
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
		HGDIOBJ hOldFont = SelectObject(hdc, hFont);
		TextOut(hdc, BORDER_W, BORDER_H, install_message, my_wcslen(install_message));
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
	my_memset(&wc, 0, sizeof(wc));

	wc.cbSize = sizeof(wc);
	wc.style = CS_NOCLOSE;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"thcrap module install popup";
	wc.hIconSm = NULL;

	RegisterClassEx(&wc);
}

static void addNonClientArea(DWORD dwStyle, int* w, int* h)
{
	RECT rc = { 0, 0, *w, *h };
	AdjustWindowRect(&rc, dwStyle, FALSE);
	*w = rc.right - rc.left;
	*h = rc.bottom - rc.top;
}

static void centerWindow(int w, int h, int* x, int* y)
{
	POINT cursor;
	GetCursorPos(&cursor);

	HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi;
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(monitor, &mi);

	*x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left) / 2 - w / 2;
	*y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top) / 2 - h / 2;
}

HWND createInstallPopup(LPWSTR install_message)
{
	HWND hwnd;

	registerClass();

	const int textWidth = 226;
	int w = textWidth + BORDER_W * 2;
	int h = 17 + BORDER_H * 2;
	int x;
	int y;
	addNonClientArea(WS_POPUPWINDOW | WS_CAPTION, &w, &h);
	centerWindow(w, h, &x, &y);

	hwnd = CreateWindow(
		L"thcrap module install popup",
		L"Touhou Community Reliant Automatic Patcher",
		WS_POPUPWINDOW | WS_CAPTION,
		x, y, w, h,
		NULL, NULL, GetModuleHandle(NULL), install_message);

	ShowWindow(hwnd, SW_SHOWDEFAULT);
	UpdateWindow(hwnd);

	return hwnd;
}
