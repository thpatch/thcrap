/**
  * Touhou Community Reliant Automatic Patcher
  * Update plugin
  *
  * ----
  *
  * Update the patches before running the game
  */

#include <thcrap.h>
#include "update.h"
#include "notify.h"
#include "self.h"

enum {
	HWND_MAIN,
	HWND_LABEL_STATUS,
	HWND_LABEL1,
	HWND_LABEL2,
	HWND_LABEL3,
	HWND_LABEL4,
	HWND_PROGRESS1,
	HWND_PROGRESS2,
	HWND_PROGRESS3,
	HWND_CHECKBOX,
	HWND_EDIT,
	HWND_UPDOWN,
	HWND_BUTTON_UPDATE,
	HWND_BUTTON_RUN,
	HWND_BUTTON_EXPAND_LOGS,
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
	bool game_started;
	bool background_updates;
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
				thcrap_inject_into_new(state->exe_fn, state->args);
			}
			break;

		case HWND_BUTTON_UPDATE:
			if (HIWORD(wParam) == BN_CLICKED) {
				SetEvent(state->event_require_update);
			}
			break;

		case HWND_CHECKBOX:
			if (HIWORD(wParam) == BN_CLICKED) {
				BOOL enable_state;
				if (SendMessage(state->hwnd[HWND_CHECKBOX], BM_GETCHECK, 0, 0) == BST_CHECKED) {
					state->background_updates = true;
					enable_state = TRUE;
				}
				else {
					state->background_updates = false;
					enable_state = FALSE;
				}
				EnableWindow(state->hwnd[HWND_LABEL4], enable_state);
				EnableWindow(state->hwnd[HWND_EDIT], enable_state);
				EnableWindow(state->hwnd[HWND_UPDOWN], enable_state);
			}
			break;

		case HWND_EDIT:
			if (HIWORD(wParam) == EN_CHANGE) {
				BOOL success;
				UINT n = GetDlgItemInt(state->hwnd[HWND_MAIN], HWND_EDIT, &success, FALSE);
				if (success) {
					EnterCriticalSection(&state->cs);
					state->time_between_updates = n;
					LeaveCriticalSection(&state->cs);
				}
			}
			break;

		case HWND_BUTTON_EXPAND_LOGS:
			if (HIWORD(wParam) == BN_CLICKED) {
				if (IsWindowVisible(state->hwnd[HWND_EDIT_LOGS])) {
					// Hide log window
					SetWindowPos(state->hwnd[HWND_MAIN], 0, 0, 0, 500, 285, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
					ShowWindow(state->hwnd[HWND_EDIT_LOGS], SW_HIDE);
					SetWindowTextW(state->hwnd[HWND_BUTTON_EXPAND_LOGS], L"Display logs");
				}
				else {
					// Show log window
					ShowWindow(state->hwnd[HWND_EDIT_LOGS], SW_SHOW);
					SetWindowPos(state->hwnd[HWND_MAIN], 0, 0, 0, 500, 390, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
					SetWindowTextW(state->hwnd[HWND_BUTTON_EXPAND_LOGS], L"Hide logs");
				}
			}
			break;
		}
		break;

	case WM_DESTROY:
		state->cancel_update = true;
		PostQuitMessage(0);
		break;
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
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

	state->hwnd[HWND_MAIN] = CreateWindowW(L"LoaderUpdateWindow", L"Touhou Community Reliant Automatic Patcher",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, 0, 500, 285, NULL, NULL, hMod, state);
	state->hwnd[HWND_LABEL_STATUS] = CreateWindowW(L"Static", L"Checking for updates...", WS_CHILD | WS_VISIBLE,
		5, 5, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_LABEL_STATUS, hMod, NULL);
	state->hwnd[HWND_LABEL1] = CreateWindowW(L"Static", L"", WS_CHILD | WS_VISIBLE,
		5, 30, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_LABEL1, hMod, NULL);
	state->hwnd[HWND_PROGRESS1] = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE,
		5, 55, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_PROGRESS1, hMod, NULL);
	state->hwnd[HWND_LABEL2] = CreateWindowW(L"Static", L"", WS_CHILD | WS_VISIBLE,
		5, 80, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_LABEL2, hMod, NULL);
	state->hwnd[HWND_PROGRESS2] = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE,
		5, 105, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_PROGRESS2, hMod, NULL);
	state->hwnd[HWND_LABEL3] = CreateWindowW(L"Static", L"", WS_CHILD | WS_VISIBLE,
		5, 130, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_LABEL3, hMod, NULL);
	state->hwnd[HWND_PROGRESS3] = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE,
		5, 155, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_PROGRESS3, hMod, NULL);
	state->hwnd[HWND_CHECKBOX] = CreateWindowW(L"Button", L"Keep the updater running in background", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		5, 180, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_CHECKBOX, hMod, NULL);
	if (state->background_updates) {
		CheckDlgButton(state->hwnd[HWND_MAIN], HWND_CHECKBOX, BST_CHECKED);
	}
	// @Nmlgc It will be nice if your smartdlg is *that* flexible
	state->hwnd[HWND_LABEL4] = CreateWindowW(L"Static", L"Check for updates every                    minutes",
		WS_CHILD | WS_VISIBLE | (state->background_updates ? 0 : WS_DISABLED),
		5, 205, 480, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_LABEL4, hMod, NULL);
	state->hwnd[HWND_EDIT] = CreateWindowW(L"Edit", L"", WS_CHILD | WS_VISIBLE | ES_NUMBER | (state->background_updates ? 0 : WS_DISABLED),
		155, 205, 35, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_EDIT, hMod, NULL);
	state->hwnd[HWND_UPDOWN] = CreateWindowW(UPDOWN_CLASSW, NULL,
		WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_NOTHOUSANDS | UDS_ARROWKEYS | (state->background_updates ? 0 : WS_DISABLED),
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, state->hwnd[HWND_MAIN], (HMENU)HWND_UPDOWN, hMod, NULL);
	SendMessage(state->hwnd[HWND_UPDOWN], UDM_SETBUDDY, (WPARAM)state->hwnd[HWND_EDIT], 0);
	SendMessage(state->hwnd[HWND_UPDOWN], UDM_SETPOS, 0, state->time_between_updates);
	SendMessage(state->hwnd[HWND_UPDOWN], UDM_SETRANGE, 0, MAKELPARAM(UD_MAXVAL, 0));
	state->hwnd[HWND_BUTTON_EXPAND_LOGS] = CreateWindowW(L"Button", L"Display logs", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		5, 230, 153, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_BUTTON_EXPAND_LOGS, hMod, NULL);
	state->hwnd[HWND_BUTTON_UPDATE] = CreateWindowW(L"Button", L"Check for updates", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
		168, 230, 153, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_BUTTON_UPDATE, hMod, NULL);
	state->hwnd[HWND_BUTTON_RUN] = CreateWindowW(L"Button", L"Run the game", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
		331, 230, 153, 18, state->hwnd[HWND_MAIN], (HMENU)HWND_BUTTON_RUN, hMod, NULL);
	state->hwnd[HWND_EDIT_LOGS] = CreateWindowW(L"Edit", L"", WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
		5, 255, 480, 100, state->hwnd[HWND_MAIN], (HMENU)HWND_EDIT_LOGS, hMod, NULL);

	if (hFont) {
		SendMessageW(state->hwnd[HWND_MAIN], WM_SETFONT, (WPARAM)hFont, 0);
		SendMessageW(state->hwnd[HWND_LABEL_STATUS], WM_SETFONT, (WPARAM)hFont, 0);
		SendMessageW(state->hwnd[HWND_LABEL1], WM_SETFONT, (WPARAM)hFont, 0);
		SendMessageW(state->hwnd[HWND_LABEL2], WM_SETFONT, (WPARAM)hFont, 0);
		SendMessageW(state->hwnd[HWND_LABEL3], WM_SETFONT, (WPARAM)hFont, 0);
		SendMessageW(state->hwnd[HWND_LABEL4], WM_SETFONT, (WPARAM)hFont, 0);
		SendMessageW(state->hwnd[HWND_CHECKBOX], WM_SETFONT, (WPARAM)hFont, 0);
		SendMessageW(state->hwnd[HWND_EDIT], WM_SETFONT, (WPARAM)hFont, 0);
		SendMessageW(state->hwnd[HWND_BUTTON_UPDATE], WM_SETFONT, (WPARAM)hFont, 0);
		SendMessageW(state->hwnd[HWND_BUTTON_RUN], WM_SETFONT, (WPARAM)hFont, 0);
		SendMessageW(state->hwnd[HWND_BUTTON_EXPAND_LOGS], WM_SETFONT, (WPARAM)hFont, 0);
		SendMessageW(state->hwnd[HWND_EDIT_LOGS], WM_SETFONT, (WPARAM)hFont, 0);
	}

	ShowWindow(state->hwnd[HWND_MAIN], SW_SHOW);
	UpdateWindow(state->hwnd[HWND_MAIN]);
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
	const char *format3 = "%s (%d o / %d o)";
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
	SetWindowTextU(state->hwnd[HWND_LABEL1], buffer);
	sprintf(buffer, format2, patch_progress, patch_total);
	SetWindowTextU(state->hwnd[HWND_LABEL2], buffer);
	sprintf(buffer, format3, fn, file_progress, file_total);
	SetWindowTextU(state->hwnd[HWND_LABEL3], buffer);

	SendMessage(state->hwnd[HWND_PROGRESS1], PBM_SETPOS, stack_progress * 100 / stack_total, 0);
	SendMessage(state->hwnd[HWND_PROGRESS2], PBM_SETPOS, patch_progress * 100 / patch_total, 0);
	SendMessage(state->hwnd[HWND_PROGRESS3], PBM_SETPOS, file_progress * 100 / file_total, 0);

	if (state->cancel_update) {
		return FALSE;
	}
	return TRUE;
}

static HWND hLogEdit;
void log_callback(const char* text)
{
	SendMessageA(hLogEdit, EM_SETSEL, 0, -1);
	SendMessageA(hLogEdit, EM_SETSEL, -1, -1);
	SendMessageA(hLogEdit, EM_REPLACESEL, 0, (LPARAM)text);
}

void log_ncallback(const char* text, size_t len)
{
	VLA(char, ntext, len + 1);
	memcpy(ntext, text, len);
	ntext[len] = '\0';
	log_callback(ntext);
	VLA_FREE(ntext);
}

BOOL loader_update_with_UI(const char *exe_fn, char *args)
{
	loader_update_state_t state;
	json_t *game;
	BOOL ret = 0;

	patches_init(json_object_get_string(runconfig_get(), "run_cfg_fn"));
	stack_show_missing();

	game = identify(exe_fn);
	if(game == nullptr) {
		return 1;
	}
	json_t *old_cfg = runconfig_get();
	json_object_merge(game, old_cfg);
	runconfig_set(game);

	InitializeCriticalSection(&state.cs);
	state.event_created = CreateEvent(nullptr, true, false, nullptr);
	state.event_require_update = CreateEvent(nullptr, false, false, nullptr);
	state.game_started = false;
	state.cancel_update = false;
	state.exe_fn = exe_fn;
	state.args = args;
	state.state = STATE_INIT;

	json_t *config = json_load_file_report("config.js");
	if (!config) {
		config = json_object();
	}
	json_t *background_updates_object = json_object_get(config, "background_updates");
	if (background_updates_object) {
		state.background_updates = json_boolean_value(background_updates_object);
	}
	else {
		state.background_updates = true;
	}
	state.time_between_updates = (int)json_integer_value(json_object_get(config, "time_between_updates"));
	if (state.time_between_updates == 0) {
		state.time_between_updates = 5;
	}

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

	stack_update(update_filter_global, NULL, loader_update_progress_callback, &state);

	// Update the thcrap engine
	size_t cur_dir_len = GetCurrentDirectory(0, nullptr);
	VLA(char, cur_dir, cur_dir_len);
	GetCurrentDirectory(cur_dir_len, cur_dir);
	json_object_set_new(runconfig_get(), "thcrap_dir", json_string(cur_dir));
	if (update_notify_thcrap() == SELF_OK) {
		// Re-run the loader
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

	SetWindowTextW(state.hwnd[HWND_LABEL_STATUS], L"Updating patch files...");
	EnableWindow(state.hwnd[HWND_BUTTON_RUN], TRUE);
	state.state = STATE_PATCHES_UPDATE;
	stack_update(update_filter_games, json_object_get(game, "game"), loader_update_progress_callback, &state);

	EnterCriticalSection(&state.cs);
	bool game_started = state.game_started;
	state.game_started = true;
	LeaveCriticalSection(&state.cs);
	if (game_started == false) {
		ret = thcrap_inject_into_new(exe_fn, args);
	}
	if (state.background_updates) {
		int time_between_updates;
		HANDLE handles[2];
		handles[0] = state.hThread;
		handles[1] = state.event_require_update;
		DWORD wait_ret;
		do {
			SetWindowTextW(state.hwnd[HWND_LABEL_STATUS], L"Updating other patches and games...");
			EnableWindow(state.hwnd[HWND_BUTTON_UPDATE], FALSE);
			global_update(loader_update_progress_callback, &state);
			EnterCriticalSection(&state.cs);
			time_between_updates = state.time_between_updates;
			LeaveCriticalSection(&state.cs);

			// Display the "Update finished" message
			EnableWindow(state.hwnd[HWND_BUTTON_UPDATE], TRUE);
			SetWindowTextW(state.hwnd[HWND_LABEL_STATUS], L"Update finished");
			SetWindowTextW(state.hwnd[HWND_LABEL1], L"");
			SetWindowTextW(state.hwnd[HWND_LABEL2], L"");
			SetWindowTextW(state.hwnd[HWND_LABEL3], L"");
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
	json_object_set_new(config, "background_updates", json_boolean(state.background_updates));
	json_object_set_new(config, "time_between_updates", json_integer(state.time_between_updates));
	json_dump_file(config, "config.js", JSON_INDENT(2) | JSON_SORT_KEYS);
	json_decref(config);

	DeleteCriticalSection(&state.cs);
	CloseHandle(state.hThread);
	CloseHandle(state.event_created);
	CloseHandle(state.event_require_update);
	json_decref(game);
	return ret;
}
