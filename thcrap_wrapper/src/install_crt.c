#include "thcrap_wrapper.h"

const int BORDER_W = 26;
const int BORDER_H = 13;

// TODO: Get these dynamically from the bundled installer
#define SuppliedCRTVersionMajor		14u
#define SuppliedCRTVersionMinor		22u
#define SuppliedCRTVersionBuild		27821u
#define SuppliedCRTVersionRebuild	0u

typedef enum {
	NotInstalled,
	NeedsUpdate,
	IsCurrent
} CrtStatus_t;

static const WCHAR* crt_install_message = L"";

static CrtStatus_t CheckCRTStatus()
{
	// Look in the registry
	HKEY Key;
	LSTATUS status;

	status = RegOpenKeyW(
		HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\X86",
		&Key
	);

	CrtStatus_t CrtStatus;

	if (status != ERROR_SUCCESS) {
		CrtStatus = NotInstalled;
	}
	else {
		DWORD key_data;
		DWORD data_size;
		DWORD reg_type;

		data_size = 4;
		status = RegQueryValueExW(Key, L"Installed", NULL, &reg_type, (LPBYTE)&key_data, &data_size);
		if (status != ERROR_SUCCESS || reg_type != REG_DWORD || key_data == 0) {
			CrtStatus = NotInstalled;
			goto CloseKeyAndReturn;
		}

		data_size = 4;
		status = RegQueryValueExW(Key, L"Major", NULL, &reg_type, (LPBYTE)&key_data, &data_size);
		if (status != ERROR_SUCCESS || reg_type != REG_DWORD || key_data < SuppliedCRTVersionMajor) {
			CrtStatus = NeedsUpdate;
			goto CloseKeyAndReturn;
		}

		data_size = 4;
		status = RegQueryValueExW(Key, L"Minor", NULL, &reg_type, (LPBYTE)&key_data, &data_size);
		if (status != ERROR_SUCCESS || reg_type != REG_DWORD || key_data < SuppliedCRTVersionMinor) {
			CrtStatus = NeedsUpdate;
			goto CloseKeyAndReturn;
		}

		data_size = 4;
		status = RegQueryValueExW(Key, L"Bld", NULL, &reg_type, (LPBYTE)&key_data, &data_size);
		if (status != ERROR_SUCCESS || reg_type != REG_DWORD || key_data < SuppliedCRTVersionBuild) {
			CrtStatus = NeedsUpdate;
			goto CloseKeyAndReturn;
		}

		data_size = 4;
		status = RegQueryValueExW(Key, L"Rbld", NULL, &reg_type, (LPBYTE)&key_data, &data_size);
		if (status != ERROR_SUCCESS || reg_type != REG_DWORD || key_data < SuppliedCRTVersionRebuild) {
			CrtStatus = NeedsUpdate;
			goto CloseKeyAndReturn;
		}

		CrtStatus = IsCurrent;
			
	CloseKeyAndReturn:
		RegCloseKey(Key);
	}
	return CrtStatus;
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
		TextOut(hdc, BORDER_W, BORDER_H, crt_install_message, my_wcslen(crt_install_message));
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

static void addNonClientArea(DWORD dwStyle, int *w, int *h)
{
	RECT rc = { 0, 0, *w, *h };
	AdjustWindowRect(&rc, dwStyle, FALSE);
	*w = rc.right - rc.left;
	*h = rc.bottom - rc.top;
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

	const int textWidth = 226;
	int w = textWidth + BORDER_W * 2;
	int h = 17 + BORDER_H * 2;
	int x;
	int y;
	addNonClientArea(WS_POPUPWINDOW | WS_CAPTION, &w, &h);
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
	switch (CheckCRTStatus()) {
		case IsCurrent:
			return;
		case NotInstalled:
			crt_install_message = L"Installing some required files, please wait...";
			break;
		case NeedsUpdate:
			crt_install_message = L"Updating some required files, please wait...";
			break;
	}

	// Start the VC runtime installer
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;

	my_memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	my_memset(&pi, 0, sizeof(pi));

	LPWSTR RtPath = my_alloc(my_wcslen(ApplicationPath) + my_wcslen(L"\"vc_redist.x86.exe\" /install /quiet /norestart") + 1, sizeof(wchar_t));
	LPWSTR p = RtPath;
	p = my_strcpy(p, L"\"");
	p = my_strcpy(p, ApplicationPath);
	p = my_strcpy(p, L"vc_redist.x86.exe");
	p = my_strcpy(p, L"\" /install /quiet /norestart");
	BOOL ret = CreateProcess(NULL, RtPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	my_free(RtPath);
	CloseHandle(pi.hThread);

	if (!ret) {
		return;
	}

	// Create a window
	HWND hwnd = createCrtInstallPopup();

	// Wait for the VC runtime installer
	MSG msg;
	my_memset(&msg, 0, sizeof(msg));
	while (1) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) && msg.message != WM_QUIT) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (msg.message == WM_QUIT) {
			PostQuitMessage(msg.wParam);
			break;
		}
		DWORD waitStatus = MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, INFINITE, QS_ALLEVENTS);
		if (waitStatus == WAIT_OBJECT_0 + 0) { // Process finished
			DestroyWindow(hwnd);
			break;
		}
	}

	// Cleanup
	CloseHandle(pi.hProcess);
}
