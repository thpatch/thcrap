/**
  * Touhou Community Reliant Automatic Patcher
  * Loader
  *
  * ----
  *
  * Update the patches before running the game
  */

#include <thcrap.h>
#include <thcrap_update_wrapper.h>
#include <thcrap_update\src\update.h>

enum {
	HWND_MAIN = 0,
	HWND_LABEL1 = 1,
	HWND_LABEL2 = 2,
	HWND_LABEL3 = 3,
	HWND_PROGRESS1 = 4,
	HWND_PROGRESS2 = 5,
	HWND_PROGRESS3 = 6,
};

static LRESULT CALLBACK update_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// param should point to an array with enough space for 7 HWND
DWORD WINAPI stack_update_window_create_and_run(LPVOID param)
{
	HMODULE hMod = GetModuleHandle(NULL);
	HWND *hWnd = (HWND*)param;

	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&icc);

	WNDCLASSW wndClass;
	memset(&wndClass, 0, sizeof(wndClass));
	wndClass.lpfnWndProc = update_proc;
	wndClass.hInstance = hMod;
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wndClass.lpszClassName = L"StackUpdateWindow";
	RegisterClassW(&wndClass);

	NONCLIENTMETRICSW nc_metrics = { sizeof(nc_metrics) };
	HFONT hFont = NULL;
	if (SystemParametersInfoW(
		SPI_GETNONCLIENTMETRICS, sizeof(nc_metrics), &nc_metrics, 0
		)) {
		hFont = CreateFontIndirectW(&nc_metrics.lfMessageFont);
	}

	hWnd[HWND_MAIN] = CreateWindowW(L"StackUpdateWindow", L"Touhou Community Reliant Automatic Patcher", WS_OVERLAPPED,
		CW_USEDEFAULT, 0, 500, 190, NULL, NULL, hMod, NULL);
	hWnd[HWND_LABEL1] = CreateWindowW(L"Static", L"", WS_CHILD | WS_VISIBLE,
		5, 5, 480, 18, hWnd[HWND_MAIN], NULL, hMod, NULL);
	hWnd[HWND_PROGRESS1] = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE,
		5, 30, 480, 18, hWnd[HWND_MAIN], NULL, hMod, NULL);
	hWnd[HWND_LABEL2] = CreateWindowW(L"Static", L"", WS_CHILD | WS_VISIBLE,
		5, 55, 480, 18, hWnd[HWND_MAIN], NULL, hMod, NULL);
	hWnd[HWND_PROGRESS2] = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE,
		5, 80, 480, 18, hWnd[HWND_MAIN], NULL, hMod, NULL);
	hWnd[HWND_LABEL3] = CreateWindowW(L"Static", L"", WS_CHILD | WS_VISIBLE,
		5, 105, 480, 18, hWnd[HWND_MAIN], NULL, hMod, NULL);
	hWnd[HWND_PROGRESS3] = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE,
		5, 130, 480, 18, hWnd[HWND_MAIN], NULL, hMod, NULL);

	if (hFont) {
		SendMessageW(hWnd[HWND_MAIN], WM_SETFONT, (WPARAM)hFont, 0);
		SendMessageW(hWnd[HWND_LABEL1], WM_SETFONT, (WPARAM)hFont, 0);
		SendMessageW(hWnd[HWND_LABEL2], WM_SETFONT, (WPARAM)hFont, 0);
		SendMessageW(hWnd[HWND_LABEL3], WM_SETFONT, (WPARAM)hFont, 0);
	}

	ShowWindow(hWnd[HWND_MAIN], SW_SHOW);
	UpdateWindow(hWnd[HWND_MAIN]);

	// We must run this in the same thread anyway, so we might as well
	// combine creation and the message loop into the same function.

	MSG msg;
	BOOL msg_ret;

	while ((msg_ret = GetMessageW(&msg, nullptr, 0, 0)) != 0) {
		if (msg_ret != -1) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	return TRUE;
}
/// ---------------------------------------


int stack_update_with_UI_progress_callback(DWORD stack_progress, DWORD stack_total, const json_t *patch, DWORD patch_progress, DWORD patch_total, const char *fn, get_result_t ret, DWORD file_progress, DWORD file_total, void *param)
{
	HWND *hWnd = (HWND*)param;
	const char *format1 = "Updating %s (%d/%d)...";
	const char *format2 = "Updating file %d/%d...";
	const char *format3 = "%s (%d o / %d o)";
	const char *patch_name = json_object_get_string(patch, "id");
	const unsigned int format1_len = strlen(format1) + strlen(patch_name) + 2 * 10 + 1;
	const unsigned int format2_len = strlen(format2) + 2 * 10 + 1;
	const unsigned int format3_len = strlen(format3) + strlen(fn) + 2 * 10 + 1;
	VLA(char, buffer, max(format1_len, max(format2_len, format3_len)));

	sprintf(buffer, format1, patch_name, stack_progress + 1, stack_total);
	SetWindowTextU(hWnd[HWND_LABEL1], buffer);
	sprintf(buffer, format2, patch_progress + 1, patch_total);
	SetWindowTextU(hWnd[HWND_LABEL2], buffer);
	sprintf(buffer, format3, fn, file_progress, file_total);
	SetWindowTextU(hWnd[HWND_LABEL3], buffer);

	SendMessage(hWnd[HWND_PROGRESS1], PBM_SETPOS, stack_progress * 100 / stack_total, 0);
	SendMessage(hWnd[HWND_PROGRESS2], PBM_SETPOS, patch_progress * 100 / patch_total, 0);
	SendMessage(hWnd[HWND_PROGRESS3], PBM_SETPOS, file_progress * 100 / file_total, 0);

	return 0;
}

void stack_update_with_UI_progress(update_filter_func_t filter_func, json_t *filter_data)
{
	HWND hwnd[7] = { 0 };
	HANDLE hThread = CreateThread(nullptr, 0, stack_update_window_create_and_run, &hwnd, 0, nullptr);
	while (hwnd[6] == 0) {
		Sleep(10);
	}
	stack_update_wrapper(filter_func, filter_data, stack_update_with_UI_progress_callback, hwnd);
	SendMessage(hwnd[HWND_MAIN], WM_CLOSE, 0, 0);
	CloseHandle(hThread);
}
