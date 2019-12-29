/**
  * Touhou Community Reliant Automatic Patcher
  * Update module
  *
  * ----
  *
  * Update the patches before running the game
  */

#include <thcrap.h>
#include "update.h"
#include "notify.h"
#include "self.h"
#include "loader_update.h"

enum {
	HWND_MAIN,
	HWND_LABEL_STATUS,
	HWND_PROGRESS1,
	HWND_PROGRESS2,
	HWND_PROGRESS3,
	HWND_CHECKBOX_UPDATE_AT_EXIT,
	HWND_STATIC_UPDATE_AT_EXIT,
	HWND_CHECKBOX_KEEP_UPDATER,
	HWND_STATIC_UPDATES_INTERVAL,
	HWND_EDIT_UPDATES_INTERVAL,
	HWND_UPDOWN,
	HWND_CHECKBOX_UPDATE_OTHERS,
	HWND_BUTTON_UPDATE,
	HWND_BUTTON_RUN,
	HWND_BUTTON_EXPAND_LOGS,
	HWND_BUTTON_DISABLE_UPDATES,
	HWND_EDIT_LOGS,
	HWND_END
};

enum update_state_t {
	STATE_INIT,           // Downloading files.js etc
	STATE_CORE_UPDATE,    // Downloading game-independant files for selected patches
	STATE_PATCHES_UPDATE, // Downloading files for the selected game in the selected patches
	STATE_GLOBAL_UPDATE,  // Downloading files for all games and all patches
	STATE_WAITING         // Everything is downloaded, background updates are enabled, waiting for the next background update
};

typedef struct {
	HWND hwnd[HWND_END];
	CRITICAL_SECTION cs;
	HANDLE event_created;
	HANDLE event_require_update;
	HANDLE hThread;
	update_state_t state;
	bool settings_visible;
	bool game_started;
	bool update_at_exit;
	bool background_updates;
	bool update_others;
	int time_between_updates;
	bool cancel_update;

	const char *exe_fn;
	char *args;
} loader_update_state_t;

static LRESULT CALLBACK loader_update_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static loader_update_state_t *state = nullptr;

	switch (uMsg) {
	case WM_CREATE: {
		CREATESTRUCTW *param = (CREATESTRUCTW*)lParam;
		state = (loader_update_state_t*)param->lpCreateParams;
		break;
	}

	case WM_COMMAND:
		switch LOWORD(wParam) {
		case HWND_BUTTON_RUN:
			if (HIWORD(wParam) == BN_CLICKED) {
				if (state->background_updates == false) {
					state->cancel_update = true;
				}
				EnterCriticalSection(&state->cs);
				state->game_started = true;
				LeaveCriticalSection(&state->cs);
				thcrap_inject_into_new(state->exe_fn, state->args, NULL, NULL);
			}
			break;

		case HWND_BUTTON_UPDATE:
			if (HIWORD(wParam) == BN_CLICKED) {
				SetEvent(state->event_require_update);
			}
			break;

		case HWND_CHECKBOX_KEEP_UPDATER:
			if (HIWORD(wParam) == BN_CLICKED) {
				BOOL enable_state;
				if (SendMessage(state->hwnd[HWND_CHECKBOX_KEEP_UPDATER], BM_GETCHECK, 0, 0) == BST_CHECKED) {
					state->background_updates = true;
					enable_state = TRUE;
				}
				else {
					state->background_updates = false;
					enable_state = FALSE;
				}
				EnableWindow(state->hwnd[HWND_STATIC_UPDATES_INTERVAL], enable_state);
				EnableWindow(state->hwnd[HWND_EDIT_UPDATES_INTERVAL], enable_state);
				EnableWindow(state->hwnd[HWND_UPDOWN], enable_state);
			}
			break;

		case HWND_EDIT_UPDATES_INTERVAL:
			if (HIWORD(wParam) == EN_CHANGE) {
				BOOL success;
				UINT n = GetDlgItemInt(state->hwnd[HWND_MAIN], HWND_EDIT_UPDATES_INTERVAL, &success, FALSE);
				if (success) {
					EnterCriticalSection(&state->cs);
					state->time_between_updates = n;
					LeaveCriticalSection(&state->cs);
				}
			}
			break;

		case HWND_CHECKBOX_UPDATE_AT_EXIT:
			if (HIWORD(wParam) == BN_CLICKED) {
				if (SendMessage(state->hwnd[HWND_CHECKBOX_UPDATE_AT_EXIT], BM_GETCHECK, 0, 0) == BST_CHECKED) {
					state->update_at_exit = true;
				}
				else {
					state->update_at_exit = false;
				}
			}
			break;

		case HWND_CHECKBOX_UPDATE_OTHERS:
			if (HIWORD(wParam) == BN_CLICKED) {
				if (SendMessage(state->hwnd[HWND_CHECKBOX_UPDATE_OTHERS], BM_GETCHECK, 0, 0) == BST_CHECKED) {
					state->update_others = true;
				}
				else {
					state->update_others = false;
				}
			}
			break;

		case HWND_BUTTON_DISABLE_UPDATES:
			if (HIWORD(wParam) == BN_CLICKED) {
				int len = GetCurrentDirectory(0, NULL);
				VLA(char, current_directory, len + 1);
				GetCurrentDirectory(len + 1, current_directory);
				if (log_mboxf(NULL, MB_YESNO, "Do you really want to completely disable updates?\n\n"
					"If you want to enable them again, you will need to run\n"
					"%s\\thcrap_enable_updates.bat",
					current_directory) == IDYES) {
					MoveFile("bin\\thcrap_update" DEBUG_OR_RELEASE ".dll", "bin\\thcrap_update_disabled" DEBUG_OR_RELEASE ".dll");
					const char *bat_file =
						"@echo off\n"
						"if not exist \"%~dp0\"\\bin\\thcrap_update" DEBUG_OR_RELEASE ".dll (\n"
						"move \"%~dp0\"\\bin\\thcrap_update_disabled" DEBUG_OR_RELEASE ".dll \"%~dp0\"\\bin\\thcrap_update" DEBUG_OR_RELEASE ".dll\n"
						"echo Updates enabled\n"
						") else (\n"
						"echo Updates are already enabled\n"
						")\n"
						"pause\n"
						"(goto) 2>nul & del \"%~f0\"\n";
					file_write("thcrap_enable_updates.bat", bat_file, strlen(bat_file));
					log_mbox(NULL, MB_OK, "Updates are now disabled.");
					PostQuitMessage(0);
				}
				VLA_FREE(current_directory);
			}
			break;

		case HWND_BUTTON_EXPAND_LOGS:
			if (HIWORD(wParam) == BN_CLICKED) {
				if (state->settings_visible) {
					// Hide log window
					SetWindowPos(state->hwnd[HWND_MAIN], 0, 0, 0, 500, 165, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
					state->settings_visible = false;
				}
				else {
					// Show log window
					SetWindowPos(state->hwnd[HWND_MAIN], 0, 0, 0, 500, 435, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
					state->settings_visible = true;
				}
			}
			break;
		}
		break;

	case WM_CTLCOLORSTATIC:
		if ((HWND)lParam == state->hwnd[HWND_LABEL_STATUS] ||
			(HWND)lParam == state->hwnd[HWND_CHECKBOX_UPDATE_AT_EXIT] ||
			(HWND)lParam == state->hwnd[HWND_CHECKBOX_KEEP_UPDATER] ||
			(HWND)lParam == state->hwnd[HWND_CHECKBOX_UPDATE_OTHERS] ||
			(HWND)lParam == state->hwnd[HWND_STATIC_UPDATES_INTERVAL] ||
			(HWND)lParam == state->hwnd[HWND_STATIC_UPDATE_AT_EXIT]) {
			HDC hdc = (HDC)wParam;
			SetTextColor(hdc, RGB(0, 0, 0));
			SetBkMode(hdc, TRANSPARENT);
			return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
		}
		break;

	case WM_DESTROY:
		state->cancel_update = true;
		PostQuitMessage(0);
		break;
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

struct ProgressBarWithText
{
	std::wstring text;
	HFONT hFont;
	ProgressBarWithText(LPCWSTR text)
		: text(text), hFont(NULL)
	{}
};

static LRESULT CALLBACK progress_bar_with_text_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubClass, DWORD_PTR dwRefData)
{
	ProgressBarWithText *self = (ProgressBarWithText*)dwRefData;

	switch (uMsg) {
	case WM_PAINT: {
		LRESULT ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);

		if (self->text.empty()) {
			return ret;
		}
		HDC hdc = GetDC(hWnd);
		RECT rect;
		GetClientRect(hWnd, &rect);

		HGDIOBJ hOldFont = SelectObject(hdc, self->hFont);
		int oldBkMode = SetBkMode(hdc, TRANSPARENT);
		DrawTextW(hdc, self->text.c_str(), self->text.length(), &rect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
		SetBkMode(hdc, oldBkMode);
		SelectObject(hdc, hOldFont);

		ReleaseDC(hWnd, hdc);
		return 0;
	}

	case WM_SETFONT:
		self->hFont = (HFONT)wParam;
		InvalidateRect(hWnd, NULL, TRUE);
		return 0;

	case WM_SETTEXT: {
		self->text = (LPCWSTR)lParam;
		InvalidateRect(hWnd, NULL, TRUE);
		return TRUE;
	}

	case WM_DESTROY:
		delete self;
		break;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void progress_bar_set_marquee(HWND hwnd, bool marquee, bool clear_text)
{
	LONG_PTR old_style = GetWindowLongPtrW(hwnd, GWL_STYLE);
	if (marquee == true && (old_style & PBS_MARQUEE) == 0) {
		SetWindowLongPtrW(hwnd, GWL_STYLE, old_style | PBS_MARQUEE);
		SendMessageW(hwnd, PBM_SETMARQUEE, TRUE, 0);
	}
	else if (marquee == false && (old_style & PBS_MARQUEE) != 0) {
		SendMessageW(hwnd, PBM_SETMARQUEE, FALSE, 0);
		SetWindowLongPtrW(hwnd, GWL_STYLE, old_style & ~PBS_MARQUEE);
	}

	if (clear_text) {
		SetWindowTextW(hwnd, L"");
	}
}

/*
** state: global window state.
** marquee: true to enable PBS_MARQUEE, false to disable it.
** clear_text: true to clear the text, false to keep it.
*/
void progress_bars_set_marquee(loader_update_state_t *state, bool marquee, bool clear_text)
{
	progress_bar_set_marquee(state->hwnd[HWND_PROGRESS1], marquee, clear_text);
	progress_bar_set_marquee(state->hwnd[HWND_PROGRESS2], marquee, clear_text);
	progress_bar_set_marquee(state->hwnd[HWND_PROGRESS3], marquee, clear_text);
}

// param should point to a loader_update_state_t object
DWORD WINAPI loader_update_window_create_and_run(LPVOID param)
{
	HMODULE hMod = GetModuleHandle(NULL);
	loader_update_state_t *state = (loader_update_state_t*)param;

	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&icc);

	WNDCLASSW wndClass;
	memset(&wndClass, 0, sizeof(wndClass));
	wndClass.lpfnWndProc = loader_update_proc;
	wndClass.hInstance = hMod;
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wndClass.lpszClassName = L"LoaderUpdateWindow";
	RegisterClassW(&wndClass);

	NONCLIENTMETRICSW nc_metrics = { sizeof(nc_metrics) };
	HFONT hFont = NULL;
	if (SystemParametersInfoW(
		SPI_GETNONCLIENTMETRICS, sizeof(nc_metrics), &nc_metrics, 0
		)) {
		hFont = CreateFontIndirectW(&nc_metrics.lfMessageFont);
	}

	// Main window
	state->hwnd[HWND_MAIN] = CreateWindowW(L"LoaderUpdateWindow", L"Touhou Community Reliant Automatic Patcher",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, 0, 500, 165, NULL, NULL, hMod, state);

	// Update UI
	state->hwnd[HWND_LABEL_STATUS] = CreateWindowW(L"Static", L"Checking for updates...", WS_CHILD | WS_VISIBLE,
		5, 5, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_LABEL_STATUS, hMod, NULL);
	state->hwnd[HWND_PROGRESS1] = CreateWindowW(PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE,
		5, 30, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_PROGRESS1, hMod, NULL);
	state->hwnd[HWND_PROGRESS2] = CreateWindowW(PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE,
		5, 55, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_PROGRESS2, hMod, NULL);
	state->hwnd[HWND_PROGRESS3] = CreateWindowW(PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE,
		5, 80, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_PROGRESS3, hMod, NULL);
	progress_bars_set_marquee(state, true, false);
	SetWindowSubclass(state->hwnd[HWND_PROGRESS1], progress_bar_with_text_proc, 1, (DWORD_PTR)new ProgressBarWithText(L""));
	SetWindowSubclass(state->hwnd[HWND_PROGRESS2], progress_bar_with_text_proc, 2, (DWORD_PTR)new ProgressBarWithText(L""));
	SetWindowSubclass(state->hwnd[HWND_PROGRESS3], progress_bar_with_text_proc, 3, (DWORD_PTR)new ProgressBarWithText(L""));

	// Buttons
	state->hwnd[HWND_BUTTON_EXPAND_LOGS] = CreateWindowW(L"Button", L"Settings and logs...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		5, 105, 153, 23, state->hwnd[HWND_MAIN], (HMENU)HWND_BUTTON_EXPAND_LOGS, hMod, NULL);
	state->hwnd[HWND_BUTTON_UPDATE] = CreateWindowW(L"Button", L"Check for updates", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
		168, 105, 153, 23, state->hwnd[HWND_MAIN], (HMENU)HWND_BUTTON_UPDATE, hMod, NULL);
	state->hwnd[HWND_BUTTON_RUN] = CreateWindowW(L"Button", L"Run the game", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
		331, 105, 153, 23, state->hwnd[HWND_MAIN], (HMENU)HWND_BUTTON_RUN, hMod, NULL);

	// Settings and logs
	state->hwnd[HWND_CHECKBOX_UPDATE_AT_EXIT] = CreateWindowW(L"Button", L"Install updates after running the game (requires a restart)",
		WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		5, 145, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_CHECKBOX_UPDATE_AT_EXIT, hMod, NULL);
	if (state->update_at_exit) {
		CheckDlgButton(state->hwnd[HWND_MAIN], HWND_CHECKBOX_UPDATE_AT_EXIT, BST_CHECKED);
	}
	state->hwnd[HWND_STATIC_UPDATE_AT_EXIT] = CreateWindowW(L"Static",
		L"If it isn't checked, the updates are installed before running the game, ensuring it is fully up to date.", WS_CHILD | WS_VISIBLE,
		5, 163, 480, 30, state->hwnd[HWND_MAIN], (HMENU)HWND_STATIC_UPDATE_AT_EXIT, hMod, NULL);
	// If it isn't checked, the updates are installed before running the game, ensuring it is fully up to date.
	state->hwnd[HWND_CHECKBOX_KEEP_UPDATER] = CreateWindowW(L"Button", L"Keep the updater running in background", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		5, 200, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_CHECKBOX_KEEP_UPDATER, hMod, NULL);
	if (state->background_updates) {
		CheckDlgButton(state->hwnd[HWND_MAIN], HWND_CHECKBOX_KEEP_UPDATER, BST_CHECKED);
	}
	state->hwnd[HWND_STATIC_UPDATES_INTERVAL] = CreateWindowW(L"Static", L"Check for updates every                    minutes",
		WS_CHILD | WS_VISIBLE | (state->background_updates ? 0 : WS_DISABLED),
		5, 218, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_STATIC_UPDATES_INTERVAL, hMod, NULL);
	state->hwnd[HWND_EDIT_UPDATES_INTERVAL] = CreateWindowW(L"Edit", L"", WS_CHILD | WS_VISIBLE | ES_NUMBER | (state->background_updates ? 0 : WS_DISABLED),
		155, 218, 35, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_EDIT_UPDATES_INTERVAL, hMod, NULL);
	state->hwnd[HWND_UPDOWN] = CreateWindowW(UPDOWN_CLASSW, NULL,
		WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_NOTHOUSANDS | UDS_ARROWKEYS | (state->background_updates ? 0 : WS_DISABLED),
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, state->hwnd[HWND_MAIN], (HMENU)HWND_UPDOWN, hMod, NULL);
	SendMessage(state->hwnd[HWND_UPDOWN], UDM_SETBUDDY, (WPARAM)state->hwnd[HWND_EDIT_UPDATES_INTERVAL], 0);
	SendMessage(state->hwnd[HWND_UPDOWN], UDM_SETPOS, 0, state->time_between_updates);
	SendMessage(state->hwnd[HWND_UPDOWN], UDM_SETRANGE, 0, MAKELPARAM(UD_MAXVAL, 0));
	state->hwnd[HWND_CHECKBOX_UPDATE_OTHERS] = CreateWindowW(L"Button", L"Update other games and patches", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		5, 245, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_CHECKBOX_UPDATE_OTHERS, hMod, NULL);
	if (state->update_others) {
		CheckDlgButton(state->hwnd[HWND_MAIN], HWND_CHECKBOX_UPDATE_OTHERS, BST_CHECKED);
	}
	state->hwnd[HWND_BUTTON_DISABLE_UPDATES] = CreateWindowW(L"Button", L"Completely disable updates (not recommended)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		5, 270, 480, 23, state->hwnd[HWND_MAIN], (HMENU)HWND_BUTTON_DISABLE_UPDATES, hMod, NULL);
	state->hwnd[HWND_EDIT_LOGS] = CreateWindowW(L"Edit", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
		5, 300, 480, 100, state->hwnd[HWND_MAIN], (HMENU)HWND_EDIT_LOGS, hMod, NULL);
	SendMessageW(state->hwnd[HWND_EDIT_LOGS], EM_LIMITTEXT, -1, 0);

	// Font
	if (hFont) {
		for (int i = 0; i < HWND_END; i++) {
			SendMessageW(state->hwnd[i], WM_SETFONT, (WPARAM)hFont, 0);
		}
	}

	ShowWindow(state->hwnd[HWND_MAIN], SW_SHOW);
	UpdateWindow(state->hwnd[HWND_MAIN]);
	log_mbox_set_owner(state->hwnd[HWND_MAIN]);
	SetEvent(state->event_created);

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

	state->cancel_update = true;
	return TRUE;
}
/// ---------------------------------------


int loader_update_progress_callback(DWORD stack_progress, DWORD stack_total, const json_t *patch, DWORD patch_progress, DWORD patch_total, const char *fn, get_result_t ret, DWORD file_progress, DWORD file_total, void *param)
{
	loader_update_state_t *state = (loader_update_state_t*)param;
	const char *format1 = "Updating %s (%d/%d)...";
	const char *format2 = "Updating file %d/%d...";
	const char *format3 = "%s (%d B / %d B)";
	const char *patch_name = json_object_get_string(patch, "id");
	const unsigned int format1_len = strlen(format1) + strlen(patch_name) + 2 * 10 + 1;
	const unsigned int format2_len = strlen(format2) + 2 * 10 + 1;
	const unsigned int format3_len = strlen(format3) + strlen(fn) + 2 * 10 + 1;
	VLA(char, buffer, max(format1_len, max(format2_len, format3_len)));

	if (state->state == STATE_INIT && ret == GET_OK) {
		SetWindowTextW(state->hwnd[HWND_LABEL_STATUS], L"Updating core files...");
		state->state = STATE_CORE_UPDATE;
	}

	sprintf(buffer, format1, patch_name, stack_progress + 1, stack_total);
	SetWindowTextU(state->hwnd[HWND_PROGRESS1], buffer);
	sprintf(buffer, format2, patch_progress, patch_total);
	SetWindowTextU(state->hwnd[HWND_PROGRESS2], buffer);
	sprintf(buffer, format3, fn, file_progress, file_total);
	SetWindowTextU(state->hwnd[HWND_PROGRESS3], buffer);

	progress_bars_set_marquee(state, false, false);
	SendMessage(state->hwnd[HWND_PROGRESS1], PBM_SETPOS, stack_total ? stack_progress * 100 / stack_total : 0, 0);
	SendMessage(state->hwnd[HWND_PROGRESS2], PBM_SETPOS, patch_total ? patch_progress * 100 / patch_total : 0, 0);
	SendMessage(state->hwnd[HWND_PROGRESS3], PBM_SETPOS, file_total  ? file_progress  * 100 / file_total  : 0, 0);

	if (state->cancel_update) {
		return FALSE;
	}
	return TRUE;
}

static HWND hLogEdit;
void log_callback(const char* text)
{
	// Select the end
	SendMessageW(hLogEdit, EM_SETSEL, 0, -1);
	SendMessageW(hLogEdit, EM_SETSEL, -1, -1);

	// Replace every \n with \r\n
	std::string text_crlf;
	do {
		const char* nl = strchr(text, '\n');
		if (!nl) {
			// No LF - append everything
			text_crlf.append(text);
			text = nl;
		}
		else if (nl > text && // There is a character before nl[0]
			nl[-1] == '\r') {
			// CRLF - no conversion needed
			nl++;
			text_crlf.append(text, nl);
			text = nl;
		}
		else {
			// LF without CR - add a CR
			text_crlf.append(text, nl);
			text_crlf += "\r\n";
			text = nl + 1;
		}
	} while (text != nullptr && text[0] != '\0');
	text = text_crlf.c_str();

	// Write the text
	WCHAR_T_DEC(text);
	WCHAR_T_CONV(text);
	SendMessageW(hLogEdit, EM_REPLACESEL, 0, (LPARAM)text_w);
	WCHAR_T_FREE(text);
}

void log_ncallback(const char* text, size_t len)
{
	VLA(char, ntext, len + 1);
	memcpy(ntext, text, len);
	ntext[len] = '\0';
	log_callback(ntext);
	VLA_FREE(ntext);
}

BOOL loader_update_with_UI(const char *exe_fn, char *args, const char *game_id_fallback)
{
	loader_update_state_t state;
	BOOL ret = 0;

	patches_init(json_object_get_string(runconfig_get(), "run_cfg_fn"));
	stack_show_missing();

	{
		json_t *game = identify(exe_fn);
		if (game) {
			json_t *old_cfg = runconfig_get();
			json_object_merge(game, old_cfg);
			runconfig_set(game);
			json_decref(game);
		}
	}

	InitializeCriticalSection(&state.cs);
	state.event_created = CreateEvent(nullptr, true, false, nullptr);
	state.event_require_update = CreateEvent(nullptr, false, false, nullptr);
	state.settings_visible = false;
	state.game_started = false;
	state.cancel_update = false;
	state.exe_fn = exe_fn;
	state.args = args;
	state.state = STATE_INIT;

	state.update_at_exit = globalconfig_get_boolean("update_at_exit", false);
	state.background_updates = globalconfig_get_boolean("background_updates", true);
	state.time_between_updates = (int)globalconfig_get_integer("time_between_updates", 5);
	state.update_others = globalconfig_get_boolean("update_others", true);
	
	SetLastError(0);
	HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(HWND), "thcrap update UI");
	bool mapExists = GetLastError() == ERROR_ALREADY_EXISTS;
	HWND *globalHwnd;
	if (hMap != nullptr) {
		globalHwnd = (HWND*)MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, sizeof(HWND));
		if (mapExists) {
			DWORD otherPid;
			GetWindowThreadProcessId(*globalHwnd, &otherPid);
			HANDLE otherProcess = OpenProcess(SYNCHRONIZE, FALSE, otherPid);
			PostMessageW(*globalHwnd, WM_CLOSE, 0, 0);
			WaitForSingleObject(otherProcess, 3000);
			CloseHandle(otherProcess);
		}
	}
	else {
		globalHwnd = nullptr;
	}

	state.hThread = CreateThread(nullptr, 0, loader_update_window_create_and_run, &state, 0, nullptr);
	WaitForSingleObject(state.event_created, INFINITE);
	if (globalHwnd) {
		*globalHwnd = state.hwnd[HWND_MAIN];
	}
	hLogEdit = state.hwnd[HWND_EDIT_LOGS];
	log_set_hook(log_callback, log_ncallback);

	if (state.update_at_exit) {
		EnterCriticalSection(&state.cs);
		// We didn't enable the "start game" button yet, the game can't be running.
		state.game_started = true;
		LeaveCriticalSection(&state.cs);

		SetWindowTextW(state.hwnd[HWND_LABEL_STATUS], L"Waiting until the game exits...");
		HANDLE hProcess;
		ret = thcrap_inject_into_new(exe_fn, args, &hProcess, NULL);
		WaitForSingleObject(hProcess, INFINITE);
	}

	// Update the thcrap engine
	size_t cur_dir_len = GetCurrentDirectory(0, nullptr);
	VLA(char, cur_dir, cur_dir_len);
	GetCurrentDirectory(cur_dir_len, cur_dir);
	json_object_set_new(runconfig_get(), "thcrap_dir", json_string(cur_dir));
	if (update_notify_thcrap() == SELF_OK && state.game_started == false) {
		// Re-run an up-to-date loader
		LPSTR commandLine = GetCommandLine();
		STARTUPINFOA sa;
		PROCESS_INFORMATION pi;
		memset(&sa, 0, sizeof(sa));
		memset(&pi, 0, sizeof(pi));
		sa.cb = sizeof(sa);
		CreateProcess(nullptr, commandLine, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &sa, &pi);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		goto end;
	}

	stack_update(update_filter_global, NULL, loader_update_progress_callback, &state);

	{
		json_t *game_fallback = nullptr;
		json_t *game = json_object_get(runconfig_get(), "game");
		if (!game) {
			game_fallback = json_string(game_id_fallback);
			game = game_fallback;
		}
		if (game) {
			SetWindowTextW(state.hwnd[HWND_LABEL_STATUS], L"Updating patch files...");
			progress_bars_set_marquee(&state, true, true);
			EnableWindow(state.hwnd[HWND_BUTTON_RUN], TRUE);
			state.state = STATE_PATCHES_UPDATE;
			stack_update(update_filter_games, game, loader_update_progress_callback, &state);
			json_decref(game_fallback);
		}
	}

	EnterCriticalSection(&state.cs);
	bool game_started = state.game_started;
	state.game_started = true;
	LeaveCriticalSection(&state.cs);
	if (game_started == false) {
		ret = thcrap_inject_into_new(exe_fn, args, NULL, NULL);
	}
	if (state.background_updates) {
		int time_between_updates;
		HANDLE handles[2];
		handles[0] = state.hThread;
		handles[1] = state.event_require_update;
		DWORD wait_ret;
		do {
			progress_bars_set_marquee(&state, true, true);
			EnableWindow(state.hwnd[HWND_BUTTON_UPDATE], FALSE);
			if (state.update_others) {
				SetWindowTextW(state.hwnd[HWND_LABEL_STATUS], L"Updating other patches and games...");
				state.state = STATE_GLOBAL_UPDATE;
				global_update(loader_update_progress_callback, &state);
			}
			else {
				json_t *game = json_object_get(runconfig_get(), "game");
				if (game) {
					SetWindowTextW(state.hwnd[HWND_LABEL_STATUS], L"Updating patch files...");
					state.state = STATE_PATCHES_UPDATE;
					stack_update(update_filter_games, game, loader_update_progress_callback, &state);
				}
			}
			EnterCriticalSection(&state.cs);
			time_between_updates = state.time_between_updates;
			LeaveCriticalSection(&state.cs);

			// Display the "Update finished" message
			EnableWindow(state.hwnd[HWND_BUTTON_UPDATE], TRUE);
			SetWindowTextW(state.hwnd[HWND_LABEL_STATUS], L"Update finished");
			state.state = STATE_WAITING;
			progress_bars_set_marquee(&state, false, true);
			SendMessage(state.hwnd[HWND_PROGRESS1], PBM_SETPOS, 100, 0);
			SendMessage(state.hwnd[HWND_PROGRESS2], PBM_SETPOS, 100, 0);
			SendMessage(state.hwnd[HWND_PROGRESS3], PBM_SETPOS, 100, 0);

			// Wait until the next update
			wait_ret = WaitForMultipleObjects(2, handles, FALSE, time_between_updates * 60 * 1000);
		} while (wait_ret == WAIT_OBJECT_0 + 1 || wait_ret == WAIT_TIMEOUT);
	}
	else {
		SendMessage(state.hwnd[HWND_MAIN], WM_CLOSE, 0, 0);
	}

	end:	
	globalconfig_set_boolean("update_at_exit", state.update_at_exit);
	globalconfig_set_boolean("background_updates", state.background_updates);
	globalconfig_set_integer("time_between_updates", state.time_between_updates);
	globalconfig_set_boolean("update_others", state.update_others);
	
	DeleteCriticalSection(&state.cs);
	CloseHandle(state.hThread);
	CloseHandle(state.event_created);
	CloseHandle(state.event_require_update);
	return ret;
}
