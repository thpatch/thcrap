/**
  * Touhou Community Reliant Automatic Patcher
  * Update module
  *
  * ----
  *
  * Update the patches before running the game
  */

#include <thcrap.h>
#include <map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <atomic>
#include "update.h"
#include "notify.h"
#include "self.h"
#include "loader_update.h"

using namespace std::literals::chrono_literals;

enum update_state_t {
	STATE_SELF,				// Checking for thcrap updates
	STATE_CORE_UPDATE,		// Downloading game-independant files for selected patches
	STATE_PATCHES_UPDATE,	// Downloading files for the selected game in the selected patches
	STATE_GLOBAL_UPDATE,	// Downloading files for all games and all patches
	STATE_WAITING			// Everything is downloaded, background updates are enabled, waiting for the next background update
};

struct loader_update_state_t {
	std::atomic<uint32_t> time_between_updates;
	std::atomic<bool> game_started = false;
	HANDLE event_wrapper_request_update;
	HANDLE event_update_finished;
	HANDLE event_update_wrapper_terminate;
	update_state_t state;
	bool update_at_exit;
	bool background_updates;
	//update_type_t update_type;
	bool update_others;
	bool cancel_update = false;
	// false when a patch update is downloading the files.js files,
	// true when all the files.js downloads are done.
	bool files_js_done;
	std::map<std::string, std::chrono::steady_clock::time_point> files;

	const char *exe_fn;
	char *args;

	/*
	inline bool update_at_exit() const {
		return this->update_type == UPDATE_AT_EXIT;
	}
	inline bool background_updates() const {
		return this->update_type == UPDATE_IN_BACKGROUND;
	}
	*/
};

enum hwnd_index_t : uint32_t {
	// Always visible
	MAIN_WINDOW,
	STATUS_LABEL,
	PROGRESS_BAR,
	RUN_BUTTON,
	SETTINGS_AND_LOGS_BUTTON,
	UPDATE_BUTTON,
	// Visible when settings are expanded
	UPDATE_AT_EXIT_CHECKBOX,
	UPDATE_AT_EXIT_LABEL,
	BACKGROUND_UPDATES_CHECKBOX,
	UPDATE_INTERVAL_LABEL,
	UPDATE_INTERVAL_TEXTBOX,
	UPDATE_INTERVAL_UPDOWN,
	UPDATE_INTERVAL_MINUTES_LABEL,
	UPDATE_OTHERS_CHECKBOX,
	DISABLE_UPDATES_BUTTON,
	LOG_OUTPUT,
	//
	HWND_COUNT
};

struct loader_window_state_t {
	HWND hwnd[HWND_COUNT];
	HWND active;
	HANDLE event_request_update;
	HANDLE hThread;
	std::wstring progress_bar_text;
	HFONT progress_bar_font;
	DWORD border_width;
	DWORD title_height;
	bool settings_visible = false;
	loader_type_t loader_type;

	inline bool window_persistent() const {
		return this->loader_type == LOADER_PERSISTENT;
	}
	inline bool enabled() const {
		return this->loader_type != LOADER_HIDDEN;
	}

	inline void close() {
		this->loader_type = LOADER_HIDDEN;
		SendMessageW(this->hwnd[MAIN_WINDOW], WM_CLOSE, 0, 0);
	}

	inline void set_control_enabled(hwnd_index_t id, bool state) const {
		EnableWindow(this->hwnd[id], state);
	}

	inline bool checkbox_state(hwnd_index_t id) const {
		return BST_CHECKED == SendMessageW(this->hwnd[id], BM_GETCHECK, 0, 0);
	}

	inline void set_status_label(const wchar_t* str) const {
		SetWindowTextW(this->hwnd[STATUS_LABEL], str);
	}

	template<bool marquee>
	inline void progress_bar_set_marquee() const {
		HWND hwnd = this->hwnd[PROGRESS_BAR];
		LONG_PTR old_style = GetWindowLongPtrW(hwnd, GWL_STYLE);
		if constexpr (marquee == true) {
			if (!(old_style & PBS_MARQUEE)) {
				SetWindowLongPtrW(hwnd, GWL_STYLE, old_style | PBS_MARQUEE);
				SendMessageW(hwnd, PBM_SETMARQUEE, TRUE, 0);
			}
		} else if (marquee == false) {
			if (old_style & PBS_MARQUEE) {
				SetWindowLongPtrW(hwnd, GWL_STYLE, old_style & ~PBS_MARQUEE);
				SendMessageW(hwnd, PBM_SETMARQUEE, FALSE, 0);
			}
		}
	}
	inline void progress_bar_set_text(const wchar_t* str) const {
		SetWindowTextW(this->hwnd[PROGRESS_BAR], L"");
	}
	inline void progress_bar_clear_text() const {
		this->progress_bar_set_text(L"");
	}
	inline void progress_bar_set_percent(uint32_t percent) const {
		SendMessageW(this->hwnd[PROGRESS_BAR], PBM_SETPOS, percent, 0);
	}

	// Defined below to access window size constants
	inline void toggle_window_size();

	inline bool handle_enter_key() const {
		HWND active_control = this->active;
		if (
			active_control == this->hwnd[UPDATE_AT_EXIT_CHECKBOX] ||
			active_control == this->hwnd[BACKGROUND_UPDATES_CHECKBOX] ||
			active_control == this->hwnd[UPDATE_OTHERS_CHECKBOX]
		) {
			SendMessageW(active_control, BM_SETCHECK, BST_CHECKED == SendMessageW(active_control, BM_GETCHECK, 0, 0) ? BST_UNCHECKED : BST_CHECKED, 0);
			return true;
		}
		return false;
	}
};

static loader_window_state_t WINDOW_STATE;
static loader_update_state_t UPDATE_STATE;

static HMODULE updater_module;
static HANDLE UPDATE_MUTEX;

/// ---------------------------------------
/// Window layout
/// ---------------------------------------

// Terrible macro jank to allow omitting the
// style fields and auto-define them as 0
#define MACRO_EVAL(...) __VA_ARGS__
#define MACRO_FIRST(arg1, ...) arg1
#define MACRO_FIRST_EVAL(...) MACRO_EVAL(MACRO_FIRST(__VA_ARGS__))
#define MACRO_AFTER_FIRST(arg1, ...) __VA_ARGS__
#define MACRO_SECOND(arg1, arg2, ...) arg2
#define MACRO_SECOND_EVAL(...) MACRO_EVAL(MACRO_SECOND(__VA_ARGS__))

static constexpr DWORD EDGE_PADDING = 5; // Distance from the edge of the window to place controls

static constexpr DWORD X_SPACING = 2; // Horizontal padding between controls
static constexpr DWORD Y_SPACING = 7; // Vertical padding between controls

static constexpr DWORD CLIENT_AREA_WIDTH = 500;

// Standard width of a control intended to take up the whole window.
static constexpr DWORD FULL_WINDOW_WIDTH = CLIENT_AREA_WIDTH - EDGE_PADDING * 2;

#define XPOS(name)		TH_MACRO_CONCAT(name,_X)
#define YPOS(name)		TH_MACRO_CONCAT(name,_Y)
#define WIDTH(name)		TH_MACRO_CONCAT(name,_W)
#define HEIGHT(name)	TH_MACRO_CONCAT(name,_H)
#define LABEL(name)		TH_MACRO_CONCAT(name,_TEXT)
#define STYLE(name)		TH_MACRO_CONCAT(name,_STYLE)
#define EXSTYLE(name)	TH_MACRO_CONCAT(name,_EXSTYLE)

#define LEFT(name)		XPOS(name)
#define TOP(name)		YPOS(name)
#define RIGHT(name)		(XPOS(name)+WIDTH(name))
#define BOTTOM(name)	(YPOS(name)+HEIGHT(name))

#define BESIDE(name)	(X_SPACING + RIGHT(name))
#define BELOW(name)		(Y_SPACING + BOTTOM(name))

// #define MAKE_CONTROL(name, x, y, w, h, text, style = 0, exstyle = 0)
#define MAKE_CONTROL(name, x, y, w, h, ...) \
static constexpr auto& LABEL(name) = MACRO_FIRST_EVAL(__VA_ARGS__); \
static constexpr DWORD XPOS(name) = x, YPOS(name) = y, WIDTH(name) = w, HEIGHT(name) = h; \
static constexpr DWORD STYLE(name) = MACRO_SECOND_EVAL(__VA_ARGS__, 0), EXSTYLE(name) = MACRO_SECOND_EVAL(MACRO_AFTER_FIRST(__VA_ARGS__), 0)
// #define MAKE_CONTROL_NO_TEXT(name, x, y, w, style = 0, exstyle = 0)
#define MAKE_CONTROL_NO_TEXT(name, x, y, w, ...) \
static constexpr DWORD XPOS(name) = x, YPOS(name) = y, WIDTH(name) = w, HEIGHT(name) = MACRO_FIRST_EVAL(__VA_ARGS__); \
static constexpr DWORD STYLE(name) = MACRO_SECOND_EVAL(__VA_ARGS__, 0), EXSTYLE(name) = MACRO_SECOND_EVAL(MACRO_AFTER_FIRST(__VA_ARGS__), 0)

MAKE_CONTROL(STATUS_LABEL,
	/*X*/ EDGE_PADDING, /*Y*/ EDGE_PADDING,
	/*W*/ FULL_WINDOW_WIDTH, /*H*/ 18,
	/*Text*/ L"Checking for thcrap updates...",
	/*Style*/ WS_VISIBLE
);
MAKE_CONTROL(PROGRESS_BAR,
	/*X*/ EDGE_PADDING, /*Y*/ BELOW(STATUS_LABEL),
	/*W*/ FULL_WINDOW_WIDTH, /*H*/ 18,
	/*Text*/ L"",
	/*Style*/ WS_VISIBLE
);
MAKE_CONTROL(RUN_BUTTON,
	/*X*/ EDGE_PADDING, /*Y*/ BELOW(PROGRESS_BAR),
	/*W*/ FULL_WINDOW_WIDTH, /*H*/ 23,
	/*Text*/ L"Run the game",
	/*Style*/ WS_VISIBLE | WS_DISABLED | WS_GROUP | WS_TABSTOP
);
MAKE_CONTROL(SETTINGS_AND_LOGS_BUTTON,
	/*X*/ EDGE_PADDING, /*Y*/ BELOW(RUN_BUTTON),
	/*W*/ FULL_WINDOW_WIDTH / 2 - X_SPACING, /*H*/ 23,
	/*Text*/ L"Settings and logs...",
	/*Style*/ WS_VISIBLE | WS_TABSTOP
);
MAKE_CONTROL(UPDATE_BUTTON,
	/*X*/ X_SPACING + BESIDE(SETTINGS_AND_LOGS_BUTTON), /*Y*/ BELOW(RUN_BUTTON),
	/*W*/ FULL_WINDOW_WIDTH / 2 - X_SPACING, /*H*/ 23,
	/*Text*/ L"Check for updates",
	/*Style*/ WS_VISIBLE | WS_DISABLED | WS_TABSTOP
);
MAKE_CONTROL(UPDATE_AT_EXIT_CHECKBOX,
	/*X*/ EDGE_PADDING, /*Y*/ BELOW(UPDATE_BUTTON),
	/*W*/ FULL_WINDOW_WIDTH, /*H*/ 18,
	/*Text*/ L"Install updates after running the game (requires a restart)",
	/*Style*/ WS_TABSTOP
);
MAKE_CONTROL(UPDATE_AT_EXIT_LABEL,
	/*X*/ EDGE_PADDING, /*Y*/ BOTTOM(UPDATE_AT_EXIT_CHECKBOX),
	/*W*/ FULL_WINDOW_WIDTH, /*H*/ 30,
	/*Text*/ L"If it isn't checked, the updates are installed before running the game, ensuring it is fully up to date."
);
MAKE_CONTROL(BACKGROUND_UPDATES_CHECKBOX,
	/*X*/ EDGE_PADDING, /*Y*/ BELOW(UPDATE_AT_EXIT_LABEL),
	/*W*/ FULL_WINDOW_WIDTH, /*H*/ 18,
	/*Text*/ L"Keep the updater running in background",
	/*Style*/ WS_TABSTOP
);
MAKE_CONTROL(UPDATE_INTERVAL_LABEL,
	/*X*/ EDGE_PADDING, /*Y*/ 1 + BOTTOM(BACKGROUND_UPDATES_CHECKBOX),
	/*W*/ FULL_WINDOW_WIDTH, /*H*/ 18,
	/*Text*/ L"Check for updates every"
);

// Reasonable default length for the interval label
// just incase getting a dynamic length fails
static constexpr DWORD UPDATE_INTERVAL_LABEL_LENGTH_DEFAULT = 130;

MAKE_CONTROL(UPDATE_INTERVAL_TEXTBOX,
	/*X*/ UPDATE_INTERVAL_LABEL_LENGTH_DEFAULT + EDGE_PADDING, /*Y*/ BOTTOM(BACKGROUND_UPDATES_CHECKBOX),
	/*W*/ 55, /*H*/ 18,
	/*Text*/ L"",
	/*Style*/ ES_NUMBER | ES_RIGHT | WS_TABSTOP,
	/*ExStyle*/ WS_EX_CLIENTEDGE
);
MAKE_CONTROL_NO_TEXT(UPDATE_INTERVAL_UPDOWN,
	/*X*/ CW_USEDEFAULT, /*Y*/ CW_USEDEFAULT,
	/*W*/ CW_USEDEFAULT, /*H*/ CW_USEDEFAULT,
	/*Style*/ UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_NOTHOUSANDS | UDS_ARROWKEYS
);
MAKE_CONTROL(UPDATE_INTERVAL_MINUTES_LABEL,
	/*X*/ BESIDE(UPDATE_INTERVAL_TEXTBOX), /*Y*/ BOTTOM(BACKGROUND_UPDATES_CHECKBOX),
	/*W*/ FULL_WINDOW_WIDTH - RIGHT(UPDATE_INTERVAL_TEXTBOX), /*H*/ 18,
	/*Text*/ L"minutes"
);

MAKE_CONTROL(UPDATE_OTHERS_CHECKBOX,
	/*X*/ EDGE_PADDING, /*Y*/ BELOW(UPDATE_INTERVAL_LABEL),
	/*W*/ FULL_WINDOW_WIDTH, /*H*/ 18,
	/*Text*/ L"Update other games and patches",
	/*Style*/ WS_TABSTOP
);
MAKE_CONTROL(DISABLE_UPDATES_BUTTON,
	/*X*/ EDGE_PADDING, /*Y*/ BELOW(UPDATE_OTHERS_CHECKBOX),
	/*W*/ FULL_WINDOW_WIDTH, /*H*/ 23,
	/*Text*/ L"Completely disable updates (not recommended)"
);
MAKE_CONTROL(LOG_OUTPUT,
	/*X*/ EDGE_PADDING, /*Y*/ BELOW(DISABLE_UPDATES_BUTTON),
	/*W*/ FULL_WINDOW_WIDTH, /*H*/ 100,
	/*Text*/ L"Updates are enabled. Initializing update UI\r\nCreating UI thread and main window... ",
	/*Style*/ WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
	/*ExStyle*/ WS_EX_CLIENTEDGE
);

#define MAIN_WINDOW_BOTTOM_COLLAPSED UPDATE_BUTTON
#define MAIN_WINDOW_BOTTOM_EXPANDED LOG_OUTPUT

// Main window height is defined after everything else because it's calculated from the control values
MAKE_CONTROL(MAIN_WINDOW,
	/*X*/ CW_USEDEFAULT, /*Y*/ 0,
	/*W*/ CLIENT_AREA_WIDTH, /*H*/ EDGE_PADDING + BOTTOM(MAIN_WINDOW_BOTTOM_COLLAPSED),
	/*Text*/ L"Touhou Community Reliant Automatic Patcher",
	/*Style*/ WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX
);
static constexpr DWORD MAIN_WINDOW_FULL_H = EDGE_PADDING + BOTTOM(MAIN_WINDOW_BOTTOM_EXPANDED);

inline void loader_window_state_t::toggle_window_size() {
	bool new_visibility = !this->settings_visible;
	this->settings_visible = new_visibility;
	SetWindowPos(
		this->hwnd[MAIN_WINDOW], 0,
		0, 0, this->border_width + MAIN_WINDOW_W, this->title_height + (new_visibility ? MAIN_WINDOW_FULL_H : MAIN_WINDOW_H),
		SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER
	);
	// Toggle visibility for controls to
	// prevent tabbing to out-of-window controls
	int new_show = new_visibility ? SW_SHOWNA : SW_HIDE;
	size_t i = UPDATE_AT_EXIT_CHECKBOX;
	do {
		ShowWindow(this->hwnd[i], new_show);
	} while (++i != HWND_COUNT);
}

/// ---------------------------------------
/// Window related functions, no need to check enabled state
/// ---------------------------------------

static LRESULT CALLBACK loader_update_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
		case WM_ACTIVATE:
			// Track the currently active control
			// so that tabbing between buttons works
			WINDOW_STATE.active = hWnd;
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case RUN_BUTTON:
					if (HIWORD(wParam) == BN_CLICKED) {
						if (!UPDATE_STATE.background_updates) {
							UPDATE_STATE.cancel_update = true;
						}
						UPDATE_STATE.game_started = true;
						thcrap_inject_into_new(UPDATE_STATE.exe_fn, UPDATE_STATE.args, NULL, NULL);
					}
					break;

				case UPDATE_BUTTON:
					if (HIWORD(wParam) == BN_CLICKED) {
						SetEvent(WINDOW_STATE.event_request_update);
					}
					break;

				case BACKGROUND_UPDATES_CHECKBOX:
					if (HIWORD(wParam) == BN_CLICKED) {
						bool check_state = WINDOW_STATE.checkbox_state(BACKGROUND_UPDATES_CHECKBOX);
						if (UPDATE_STATE.background_updates != check_state) {
							UPDATE_STATE.background_updates = check_state;
							globalconfig_set_boolean("background_updates", check_state);
						}
						WINDOW_STATE.set_control_enabled(UPDATE_INTERVAL_LABEL, check_state);
						WINDOW_STATE.set_control_enabled(UPDATE_INTERVAL_TEXTBOX, check_state);
						WINDOW_STATE.set_control_enabled(UPDATE_INTERVAL_UPDOWN, check_state);
						WINDOW_STATE.set_control_enabled(UPDATE_INTERVAL_MINUTES_LABEL, check_state);
					}
					break;

				case UPDATE_INTERVAL_TEXTBOX:
					if (HIWORD(wParam) == EN_CHANGE) {
						BOOL success;
						UINT interval = GetDlgItemInt(WINDOW_STATE.hwnd[MAIN_WINDOW], UPDATE_INTERVAL_TEXTBOX, &success, FALSE);
						if (
							success &&
							interval != UPDATE_STATE.time_between_updates.exchange(interval)
						) {
							globalconfig_set_integer("time_between_updates", interval);
						}
					}
					break;

				case UPDATE_AT_EXIT_CHECKBOX:
					if (HIWORD(wParam) == BN_CLICKED) {
						bool check_state = WINDOW_STATE.checkbox_state(UPDATE_AT_EXIT_CHECKBOX);
						if (UPDATE_STATE.update_at_exit != check_state) {
							UPDATE_STATE.update_at_exit = check_state;
							globalconfig_set_boolean("update_at_exit", check_state);
						}
					}
					break;

				case UPDATE_OTHERS_CHECKBOX:
					if (HIWORD(wParam) == BN_CLICKED) {
						bool check_state = WINDOW_STATE.checkbox_state(UPDATE_OTHERS_CHECKBOX);
						if (UPDATE_STATE.update_others != check_state) {
							UPDATE_STATE.update_others = check_state;
							globalconfig_set_boolean("update_others", check_state);
						}
					}
					break;

				case DISABLE_UPDATES_BUTTON:
					if (HIWORD(wParam) == BN_CLICKED) {
						DWORD len = GetCurrentDirectoryU(0, NULL);
						VLA(char, current_directory, len);
						GetCurrentDirectoryU(len, current_directory);
						if (log_mboxf(NULL, MB_YESNO,
							"Do you really want to completely disable updates?\n\n"
							"If you want to enable them again, you will need to run\n"
							"%s\\thcrap_enable_updates.bat\n"
							"(this file will be created after you click yes)",
							current_directory) == IDYES
						) {
							MoveFileExW(L"bin\\thcrap_update" FILE_SUFFIX_W L".dll", L"bin\\thcrap_update_disabled" FILE_SUFFIX_W L".dll", MOVEFILE_REPLACE_EXISTING);
							static constexpr char bat_file[] =
								"@echo off\n"
								"if not exist \"%~dp0\"\\bin\\thcrap_update" FILE_SUFFIX ".dll (\n"
								"move \"%~dp0\"\\bin\\thcrap_update_disabled" FILE_SUFFIX ".dll \"%~dp0\"\\bin\\thcrap_update" FILE_SUFFIX ".dll\n"
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

				case SETTINGS_AND_LOGS_BUTTON:
					if (HIWORD(wParam) == BN_CLICKED) {
						WINDOW_STATE.toggle_window_size();
					}
					break;
			}
			break;

			// FIXME: Checkboxes don't respond to the enter key by default,
			// but this isn't getting any key related messages to fix it.
			/*
		case WM_KEYDOWN:
			log_mbox("", 0, "ENTER");
			if (wParam == VK_RETURN && !(lParam & 0x40000000)) {
				if (WINDOW_STATE.handle_enter_key()) {
					return 0;
				}
			}
			break;
			*/

		case WM_CTLCOLORSTATIC:
			if (
				(HWND)lParam == WINDOW_STATE.hwnd[STATUS_LABEL] ||
				(HWND)lParam == WINDOW_STATE.hwnd[UPDATE_AT_EXIT_CHECKBOX] ||
				(HWND)lParam == WINDOW_STATE.hwnd[BACKGROUND_UPDATES_CHECKBOX] ||
				(HWND)lParam == WINDOW_STATE.hwnd[UPDATE_OTHERS_CHECKBOX] ||
				(HWND)lParam == WINDOW_STATE.hwnd[UPDATE_INTERVAL_LABEL] ||
				(HWND)lParam == WINDOW_STATE.hwnd[UPDATE_AT_EXIT_LABEL] ||
				(HWND)lParam == WINDOW_STATE.hwnd[UPDATE_INTERVAL_MINUTES_LABEL]
			) {
				HDC hdc = (HDC)wParam;
				SetTextColor(hdc, RGB(0, 0, 0));
				SetBkMode(hdc, TRANSPARENT);
				return (INT_PTR)GetSysColorBrush(COLOR_WINDOW); // Docs say to use INT_PTR? Sure, why not
			}
			break;

		case WM_DESTROY:
			UPDATE_STATE.cancel_update = true;
			PostQuitMessage(0);
			break;
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK progress_bar_with_text_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch (uMsg) {
		case WM_PAINT: {
			LRESULT ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);

			if (WINDOW_STATE.progress_bar_text.empty()) {
				return ret;
			}
			RECT rect;
			GetClientRect(hWnd, &rect);

			HDC hdc = GetDC(hWnd);
			HGDIOBJ hOldFont = SelectObject(hdc, WINDOW_STATE.progress_bar_font);
			int oldBkMode = SetBkMode(hdc, TRANSPARENT);
			DrawTextW(hdc, WINDOW_STATE.progress_bar_text.data(), WINDOW_STATE.progress_bar_text.length(), &rect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
			SetBkMode(hdc, oldBkMode);
			SelectObject(hdc, hOldFont);

			ReleaseDC(hWnd, hdc);
			return 0;
		}

		case WM_SETFONT:
			WINDOW_STATE.progress_bar_font = (HFONT)wParam;
			InvalidateRect(hWnd, NULL, TRUE);
			return 0;

		case WM_SETTEXT:
			WINDOW_STATE.progress_bar_text = (LPCWSTR)lParam;
			InvalidateRect(hWnd, NULL, TRUE);
			return TRUE;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

HICON get_configure_icon()
{
	HMODULE hConfigure = LoadLibraryExW(L"thcrap" FILE_SUFFIX_W L".exe", NULL, LOAD_LIBRARY_AS_DATAFILE);
	if (!hConfigure) {
		return NULL;
	}

	LPWSTR iconGroupId = GetIconGroupResourceId(hConfigure);
	if (!iconGroupId) {
		FreeLibrary(hConfigure);
		return NULL;
	}

	HICON hIcon = LoadIconW(hConfigure, iconGroupId);
	if (!IS_INTRESOURCE(iconGroupId)) {
		free(iconGroupId);
	}

	return hIcon;
	// Intentionally leaking hConfigure because we need it until our main
	// window is destroyed, which is the entire lifetime of the process.
}

DWORD WINAPI loader_update_window_create_and_run(LPVOID param)
{
	{
		// For some reason putting this struct in its
		// own scope makes the compiler use 8 bytes less stack
		INITCOMMONCONTROLSEX icc;
		icc.dwSize = sizeof(icc);
		icc.dwICC = ICC_PROGRESS_CLASS;
		InitCommonControlsEx(&icc);
	}

	HMODULE hMod = GetModuleHandleW(NULL);

	WNDCLASSW wndClass = {};
	wndClass.lpfnWndProc = loader_update_proc;
	wndClass.hInstance = hMod;
	wndClass.hIcon = get_configure_icon();
	wndClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wndClass.lpszClassName = L"LoaderUpdateWindow";
	RegisterClassW(&wndClass);

#if WINVER >= 0x0600
	// Docs say to exclude the size of the padded border field for pre-vista.
	// We don't read it anyway, so just always subtract it.
	constexpr DWORD NONCLIENTMETRICSW_SIZE = sizeof(NONCLIENTMETRICSW) - sizeof(NONCLIENTMETRICSW::iPaddedBorderWidth);
#else
	constexpr DWORD NONCLIENTMETRICSW_SIZE = sizeof(NONCLIENTMETRICSW);
#endif

	HFONT hFont = NULL;
	NONCLIENTMETRICSW nc_metrics = {};
	nc_metrics.cbSize = NONCLIENTMETRICSW_SIZE;
	if (SystemParametersInfoW(
		SPI_GETNONCLIENTMETRICS, NONCLIENTMETRICSW_SIZE, &nc_metrics, 0
	)) {
		hFont = CreateFontIndirectW(&nc_metrics.lfMessageFont);
	}

	// NOTE: Even though NONCLIENTMETRICS *sounds* like it would have all
	// the necessary info to account for window borders and such, it's
	// horrendously inconsistent across different OS versions (and wine)
	// while still requiring a magic +1 offset. This function just makes
	// windows calculate the necessary values for us instead.
	RECT window_sizing_is_awful = { 0, 0, WIDTH(MAIN_WINDOW), HEIGHT(MAIN_WINDOW) };
	AdjustWindowRectEx(&window_sizing_is_awful, STYLE(MAIN_WINDOW), FALSE, EXSTYLE(MAIN_WINDOW));
	DWORD border_width = (window_sizing_is_awful.right - window_sizing_is_awful.left) - WIDTH(MAIN_WINDOW);
	DWORD title_height = (window_sizing_is_awful.bottom - window_sizing_is_awful.top) - HEIGHT(MAIN_WINDOW);
	WINDOW_STATE.border_width = border_width;
	WINDOW_STATE.title_height = title_height;

	// Main window
	HWND main_window = CreateWindowExW(
		EXSTYLE(MAIN_WINDOW),
		L"LoaderUpdateWindow", LABEL(MAIN_WINDOW),
		STYLE(MAIN_WINDOW),
		XPOS(MAIN_WINDOW), YPOS(MAIN_WINDOW), WIDTH(MAIN_WINDOW) + border_width, HEIGHT(MAIN_WINDOW) + title_height,
		NULL, NULL, hMod, NULL
	);
	WINDOW_STATE.hwnd[MAIN_WINDOW] = main_window;

#define CreateLabelStyled(name, style) CreateWindowExW(EXSTYLE(name), L"Static", LABEL(name), WS_CHILD | style | STYLE(name), XPOS(name), YPOS(name), WIDTH(name), HEIGHT(name), main_window, (HMENU)name, hMod, NULL)
#define CreateButtonStyled(name, style) CreateWindowExW(EXSTYLE(name), L"Button", LABEL(name), WS_CHILD | BS_PUSHBUTTON | style | STYLE(name), XPOS(name), YPOS(name), WIDTH(name), HEIGHT(name), main_window, (HMENU)name, hMod, NULL)
#define CreateProgressBarStyled(name, style) CreateWindowExW(EXSTYLE(name), PROGRESS_CLASSW, LABEL(name), WS_CHILD | style | STYLE(name), XPOS(name), YPOS(name), WIDTH(name), HEIGHT(name), main_window, (HMENU)name, hMod, NULL)
#define CreateCheckboxStyled(name, style) CreateWindowExW(EXSTYLE(name), L"Button", LABEL(name), WS_CHILD | BS_AUTOCHECKBOX | style | STYLE(name), XPOS(name), YPOS(name), WIDTH(name), HEIGHT(name), main_window, (HMENU)name, hMod, NULL)
#define CreateTextboxStyled(name, style) CreateWindowExW(EXSTYLE(name), L"Edit", LABEL(name), WS_CHILD | style | STYLE(name), XPOS(name), YPOS(name), WIDTH(name), HEIGHT(name), main_window, (HMENU)name, hMod, NULL)
#define CreateUpdownStyled(name, style) CreateWindowExW(EXSTYLE(name), UPDOWN_CLASSW, NULL, WS_CHILD | style | STYLE(name), XPOS(name), YPOS(name), WIDTH(name), HEIGHT(name), main_window, (HMENU)name, hMod, NULL)
#define CreateRadioButtonStyled(name, style) CreateWindowExW(EXSTYLE(name), L"Button", LABEL(name), WS_CHILD | BS_AUTORADIOBUTTON | style | STYLE(name), XPOS(name), YPOS(name), WIDTH(name), HEIGHT(name), main_window, (HMENU)name, hMod, NULL)

#define CreateLabel(name) CreateLabelStyled(name, 0)
#define CreateButton(name) CreateButtonStyled(name, 0)
#define CreateProgressBar(name) CreateProgressBarStyled(name, 0)
#define CreateCheckbox(name) CreateCheckboxStyled(name, 0)
#define CreateTextbox(name) CreateTextboxStyled(name, 0)
#define CreateUpdown(name) CreateUpdownStyled(name, 0)
#define CreateRadioButton(name) CreateRadioButtonStyled(name, 0)

	// Update UI
	WINDOW_STATE.hwnd[STATUS_LABEL] = CreateLabel(STATUS_LABEL);
	WINDOW_STATE.hwnd[PROGRESS_BAR] = CreateProgressBar(PROGRESS_BAR);
	WINDOW_STATE.progress_bar_set_marquee<true>();
	SetWindowSubclass(WINDOW_STATE.hwnd[PROGRESS_BAR], progress_bar_with_text_proc, 1, 0);

	// Main Buttons
	WINDOW_STATE.hwnd[RUN_BUTTON] = WINDOW_STATE.active = CreateButton(RUN_BUTTON);
	WINDOW_STATE.hwnd[SETTINGS_AND_LOGS_BUTTON] = CreateButton(SETTINGS_AND_LOGS_BUTTON);
	WINDOW_STATE.hwnd[UPDATE_BUTTON] = CreateButton(UPDATE_BUTTON);

	// Settings and logs
	WINDOW_STATE.hwnd[UPDATE_AT_EXIT_CHECKBOX] = CreateCheckbox(UPDATE_AT_EXIT_CHECKBOX);
	if (UPDATE_STATE.update_at_exit) {
		CheckDlgButton(main_window, UPDATE_AT_EXIT_CHECKBOX, BST_CHECKED);
	}
	WINDOW_STATE.hwnd[UPDATE_AT_EXIT_LABEL] = CreateLabel(UPDATE_AT_EXIT_LABEL);

	// If it isn't checked, the updates are installed before running the game, ensuring it is fully up to date.
	WINDOW_STATE.hwnd[BACKGROUND_UPDATES_CHECKBOX] = CreateCheckbox(BACKGROUND_UPDATES_CHECKBOX);
	if (UPDATE_STATE.background_updates) {
		CheckDlgButton(main_window, BACKGROUND_UPDATES_CHECKBOX, BST_CHECKED);
	}

	DWORD background_updates_style = UPDATE_STATE.background_updates ? 0 : WS_DISABLED;

	HWND update_interval_hwnd = CreateLabelStyled(UPDATE_INTERVAL_LABEL, background_updates_style);
	WINDOW_STATE.hwnd[UPDATE_INTERVAL_LABEL] = update_interval_hwnd;

	// Calculate the length of the update interval text to position the text box properly
	HDC hdc = GetDC(update_interval_hwnd);
	HFONT font_for_width = hFont;
	if (!hFont) {
		// If we don't already have a font then get whatever the default one is
		font_for_width = (HFONT)SendMessageW(update_interval_hwnd, WM_GETFONT, 0, 0);
	}
	font_for_width = (HFONT)SelectObject(hdc, (HGDIOBJ)font_for_width);

	DWORD UPDATE_INTERVAL_TEXTBOX_X = ::UPDATE_INTERVAL_TEXTBOX_X;
	DWORD UPDATE_INTERVAL_MINUTES_LABEL_X = ::UPDATE_INTERVAL_MINUTES_LABEL_X;
	SIZE text_size;
	if (GetTextExtentPoint32W(hdc, UPDATE_INTERVAL_LABEL_TEXT, elementsof(UPDATE_INTERVAL_LABEL_TEXT) - 1, &text_size)) {
		UPDATE_INTERVAL_TEXTBOX_X = EDGE_PADDING + text_size.cx + X_SPACING;
		UPDATE_INTERVAL_MINUTES_LABEL_X = BESIDE(UPDATE_INTERVAL_TEXTBOX);
	}

	SelectObject(hdc, (HGDIOBJ)font_for_width);
	ReleaseDC(update_interval_hwnd, hdc);

	HWND update_interval_textbox_hwnd = CreateTextboxStyled(UPDATE_INTERVAL_TEXTBOX, background_updates_style);
	WINDOW_STATE.hwnd[UPDATE_INTERVAL_TEXTBOX] = update_interval_textbox_hwnd;

	// TODO: Each click on the updown re-selects the text contents, which looks stupid
	HWND updown_hwnd = CreateUpdownStyled(UPDATE_INTERVAL_UPDOWN, background_updates_style);
	WINDOW_STATE.hwnd[UPDATE_INTERVAL_UPDOWN] = updown_hwnd;
	SendMessageW(updown_hwnd, UDM_SETBUDDY, (WPARAM)update_interval_textbox_hwnd, 0);
	SendMessageW(updown_hwnd, UDM_SETPOS, 0, UPDATE_STATE.time_between_updates);
	SendMessageW(updown_hwnd, UDM_SETRANGE, 0, MAKELPARAM(UD_MAXVAL, 0));

	WINDOW_STATE.hwnd[UPDATE_INTERVAL_MINUTES_LABEL] = CreateLabel(UPDATE_INTERVAL_MINUTES_LABEL);

	WINDOW_STATE.hwnd[UPDATE_OTHERS_CHECKBOX] = CreateCheckbox(UPDATE_OTHERS_CHECKBOX);
	if (UPDATE_STATE.update_others) {
		CheckDlgButton(main_window, UPDATE_OTHERS_CHECKBOX, BST_CHECKED);
	}

	WINDOW_STATE.hwnd[DISABLE_UPDATES_BUTTON] = CreateButton(DISABLE_UPDATES_BUTTON);

	HWND log_hwnd = CreateTextbox(LOG_OUTPUT);
	WINDOW_STATE.hwnd[LOG_OUTPUT] = log_hwnd;
	SendMessageW(log_hwnd, EM_LIMITTEXT, -1, 0);

	// Font
	if (hFont) {
		size_t i = 0;
		do {
			SendMessageW(WINDOW_STATE.hwnd[i], WM_SETFONT, (WPARAM)hFont, 0);
		} while (++i < HWND_COUNT);
	}

	ShowWindow(main_window, SW_SHOW);
	UpdateWindow(main_window);
	log_mbox_set_owner(main_window);

	// Reusing the request update event to signal for window creation
	SetEvent(WINDOW_STATE.event_request_update);

	// We must run this in the same thread anyway, so we might as well
	// combine creation and the message loop into the same function.
	for (MSG msg;;) {
		switch (GetMessageW(&msg, NULL, 0, 0)) {
			case 0: // WM_QUIT
				UPDATE_STATE.cancel_update = true;
				return TRUE;
			default:
				if (!IsDialogMessageW(WINDOW_STATE.active, &msg)) {
					TranslateMessage(&msg);
					DispatchMessageW(&msg);
				}
			case -1:
				break;
		}
	}
}

void log_callback(const char* text)
{
	// Select the end
	SendMessageW(WINDOW_STATE.hwnd[LOG_OUTPUT], EM_SETSEL, 0, -1);
	SendMessageW(WINDOW_STATE.hwnd[LOG_OUTPUT], EM_SETSEL, -1, -1);

	// Replace every \n with \r\n
	std::string text_crlf;
	do {
		const char* nl = strchr(text, '\n');
		if (!nl) {
			// No LF - append everything
			text_crlf.append(text);
			text = NULL;
		}
		else if (
			nl > text && // There is a character before nl[0]
			nl[-1] == '\r'
		) {
			// CRLF - no conversion needed
			nl++;
			text_crlf.append(text, nl);
			text = nl;
		}
		else {
			// LF without CR - add a CR
			text_crlf.append(text, nl);
			text_crlf += "\r\n"sv;
			text = nl + 1;
		}
	} while (text != NULL && text[0] != '\0');
	text = text_crlf.c_str();

	// Write the text
	WCHAR_T_DEC(text);
	WCHAR_T_CONV(text);
	SendMessageW(WINDOW_STATE.hwnd[LOG_OUTPUT], EM_REPLACESEL, 0, (LPARAM)text_w);
	WCHAR_T_FREE(text);

	// Sometimes the log doesn't scroll down all the way...? This fixes it
	SendMessageW(WINDOW_STATE.hwnd[LOG_OUTPUT], EM_SCROLLCARET, 0, 0);
}

void log_ncallback(const char* text, size_t len)
{
	VLA(char, ntext, len + 1);
	ntext[len] = '\0';
	memcpy(ntext, text, len);
	log_callback(ntext);
	VLA_FREE(ntext);
}

/// ---------------------------------------


void loader_update_progress_init(update_state_t new_state)
{
	if (WINDOW_STATE.enabled()) {
		WINDOW_STATE.set_status_label(L"Downloading files list...");
		WINDOW_STATE.progress_bar_set_marquee<true>();
		WINDOW_STATE.progress_bar_clear_text();
	}
	UPDATE_STATE.files_js_done = false;
	UPDATE_STATE.state = new_state;
}

bool loader_update_progress_callback(progress_callback_status_t *status, void* param)
{
	WaitForSingleObject(UPDATE_MUTEX, INFINITE);

	if (status->nb_files_total > 0 && UPDATE_STATE.files_js_done == false) {
		if (WINDOW_STATE.enabled()) {
			switch (UPDATE_STATE.state) {
				case STATE_CORE_UPDATE:
					WINDOW_STATE.set_status_label(L"Updating core files...");
					break;

				case STATE_PATCHES_UPDATE:
					WINDOW_STATE.set_status_label(L"Updating patch files...");
					break;

				case STATE_GLOBAL_UPDATE:
					WINDOW_STATE.set_status_label(L"Updating other patches and games...");
					break;

				case STATE_SELF:
				case STATE_WAITING:
				default:
					// Shouldn't happen
					break;
			}
			WINDOW_STATE.progress_bar_set_marquee<false>();
			WINDOW_STATE.progress_bar_set_percent(0);
		}
		UPDATE_STATE.files_js_done = true;
	}

	switch (status->status) {
		case GET_DOWNLOADING: {
			// Using the URL instead of the filename is important, because we may
			// be downloading the same file from 2 different URLs, and the UI
			// could quickly become very confusing, with progress going backwards etc.
			auto& file_time = UPDATE_STATE.files[status->url];
			auto now = std::chrono::steady_clock::now();
			if (file_time.time_since_epoch() == 0ms) {
				file_time = now;
			}
			else if (now - file_time > 5s) {
				file_time = now;
				log_printf("[%zu/%zu] %s: in progress (%zub/%zub)...\n", status->nb_files_downloaded, status->nb_files_total,
					status->url, status->file_progress, status->file_size);
			}
			break;
		}

		case GET_OK: {
			log_printf("[%zu/%zu] %s/%s: OK (%zub)\n", status->nb_files_downloaded, status->nb_files_total, status->patch->id, status->fn, status->file_size);
			if (WINDOW_STATE.enabled()) {
				const wchar_t* format = status->nb_files_total ? L"%zu/%zu" : L"%zu/???";
				FORMAT_VLA_STR(text, format, status->nb_files_downloaded, status->nb_files_total);
				if (status->nb_files_total != 0) {
					WINDOW_STATE.progress_bar_set_percent(status->nb_files_downloaded * 100 / status->nb_files_total);
				}
				WINDOW_STATE.progress_bar_set_text(text);
				VLA_FREE(text);
			}
			break;
		}
		
		case GET_CLIENT_ERROR:
		case GET_SERVER_ERROR:
		case GET_SYSTEM_ERROR:
			log_printf("%s: %s\n", status->url, status->error);
			break;
		case GET_CRC32_ERROR:
			log_printf("%s: CRC32 error\n", status->url);
			break;
		case GET_CANCELLED:
			// Another copy of the file have been downloader earlier. Ignore.
			break;
		default:
			log_printf("%s: unknown status\n", status->url);
			break;
	}

	ReleaseMutex(UPDATE_MUTEX);
	return !UPDATE_STATE.cancel_update;
}

const char* game_id_other = NULL;

// WIP code to get rid of the mailslot that doesn't work well on wine
/*
enum LoaderMessageID : int32_t {
	LoaderAddProcess = 0,
	LoaderRemoveProcess = 1,
	LoaderUpdateGame = 2
};

struct LoaderMessage {
	LoaderMessageID id;
	uint32_t buffer_length;
	union {
		DWORD pid;
		struct {
			size_t string_length;
			const char string[];
		};
	};
};

DWORD WINAPI handle_message_from_process(LPVOID parameter) {
	const LoaderMessage* message = (LoaderMessage*)parameter;
	switch (message->id) {
		case LoaderAddProcess:
		case LoaderRemoveProcess:
		case LoaderUpdateGame:
			;
	}
}
*/

static DWORD WINAPI update_wrapper_patch(void* param) {

	HANDLE hMail = CreateMailslotW((L"\\\\.\\mailslot\\thcrap_request_update_"sv + std::to_wstring(runconfig_loader_pid_get())).c_str(), 1024, MAILSLOT_WAIT_FOREVER, NULL);
	OVERLAPPED overlapped = {};
	overlapped.hEvent = CreateEventW(NULL, false, false, NULL);

	HANDLE handles[] = {
		WINDOW_STATE.hThread,
		overlapped.hEvent,
		UPDATE_STATE.event_update_wrapper_terminate
	};

	for (;;) {
		char json_str[1024] = {};
		ReadFile(hMail, json_str, sizeof(json_str), NULL, &overlapped);
		DWORD signal = WaitForMultipleObjects(elementsof(handles), handles, FALSE, INFINITE);
		if (signal != WAIT_OBJECT_0 + 1) {
			break;
		}
		json_t* json_data = json_loads(json_str, 0, NULL);
		if (!json_data) {
			break;
		}
		const wchar_t* event_name = (wchar_t*)utf8_to_utf16(json_object_get_string(json_data, "event_name"));

		WINDOW_STATE.set_control_enabled(UPDATE_BUTTON, false);
		if (game_id_other) {
			free((void*)game_id_other);
		}
		game_id_other = json_object_get_string_copy(json_data, "game_id");

		SetEvent(UPDATE_STATE.event_wrapper_request_update);

		if (!globalconfig_get_boolean("update_at_exit", false)) {
			WaitForSingleObject(UPDATE_STATE.event_update_finished, INFINITE);
		}

		HANDLE hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, event_name);
		if (hEvent) {
			SetEvent(hEvent);
			CloseHandle(hEvent);
		}
		if (!globalconfig_get_boolean("update_at_exit", false)) {
			if (UPDATE_STATE.background_updates) {
				WINDOW_STATE.set_control_enabled(UPDATE_BUTTON, true);
				WINDOW_STATE.set_control_enabled(RUN_BUTTON, true);
			} else {
				WINDOW_STATE.close();
			}
		}
		free((void*)event_name);
		json_decref(json_data);
	}
	CloseHandle(hMail);
	CloseHandle(overlapped.hEvent);
	return 0;
}

static constexpr wchar_t ID_SUFFIX[] = L"thcrap_update_dir_lock";
static_assert(elementsof(ID_SUFFIX) >= elementsof(L"thcrap_update_64_d.dll"), "ID_SUFFIX should be at least as long as \"thcrap_update_64_d.dll\"");

BOOL loader_update_with_UI(const char *exe_fn, char *args)
{
	log_print("Updates are enabled. Initializing update UI\n");
	{
		// This isn't necessarily supposed to be the filename/path,
		// just a string <= MAX_PATH that is likely to uniquely
		// identify thcrap_update instances running from a particular
		// directory.
		wchar_t path_id[MAX_PATH];
		DWORD path_id_length = GetModuleFileNameW(updater_module, path_id, MAX_PATH);
		DWORD path_id_suffix_write = std::min<DWORD>(path_id_length, MAX_PATH - elementsof(ID_SUFFIX));
		memcpy(&path_id[path_id_suffix_write], ID_SUFFIX, sizeof(ID_SUFFIX));
		UPDATE_MUTEX = CreateMutexW(NULL, FALSE, path_id);
	}

	DWORD loader_pid = GetCurrentProcessId();
	runconfig_loader_pid_set(loader_pid);

	BOOL ret = 0;

	stack_show_missing();

	{
		json_t *game = identify(exe_fn);
		if (game) {
			runconfig_load(game, RUNCONFIG_NO_OVERWRITE | RUNCONFIG_NO_BINHACKS);
			json_decref(game);
		}
	}

	UPDATE_STATE.event_wrapper_request_update = CreateEventW(NULL, false, false, NULL);
	UPDATE_STATE.event_update_finished = CreateEventW(NULL, false, false, NULL);
	UPDATE_STATE.event_update_wrapper_terminate = CreateEventW(NULL, false, false, NULL);
	UPDATE_STATE.exe_fn = exe_fn;
	UPDATE_STATE.args = args;
	UPDATE_STATE.state = STATE_SELF;

	//UPDATE_STATE.update_type = globalconfig_get_integer("update_type", DEFAULT_UPDATE_TYPE);

	UPDATE_STATE.update_at_exit = globalconfig_get_boolean("update_at_exit", false);
	UPDATE_STATE.background_updates = globalconfig_get_boolean("background_updates", false);
	UPDATE_STATE.time_between_updates = (uint32_t)globalconfig_get_integer("time_between_updates", 5);
	UPDATE_STATE.update_others = globalconfig_get_boolean("update_others", true);

#if ENABLE_OVERHAULED_UPDATE_SETTINGS
	WINDOW_STATE.loader_type = globalconfig_get_boolean("loader_type", DEFAULT_LOADER_TYPE);
#else
	WINDOW_STATE.loader_type = UPDATE_STATE.background_updates ? LOADER_PERSISTENT : LOADER_STARTUP_ONLY;
#endif

	if (WINDOW_STATE.enabled()) {
		log_print("Creating UI thread and main window... ");
		// Reusing the request update event to signal for window creation
		WINDOW_STATE.event_request_update = CreateEventW(NULL, FALSE, FALSE, NULL);
		WINDOW_STATE.hThread = CreateThread(NULL, 0, loader_update_window_create_and_run, NULL, 0, NULL);
		WaitForSingleObject(WINDOW_STATE.event_request_update, INFINITE);
		log_set_hook(log_callback, log_ncallback);
		log_print("done.\n");
	}

	HANDLE hWrapperUpdateThread = CreateThread(NULL, 0, update_wrapper_patch, NULL, 0, NULL);
	if (UPDATE_STATE.update_at_exit) {
		// We didn't enable the "start game" button yet, the game can't be running.
		UPDATE_STATE.game_started = true;

		if (WINDOW_STATE.enabled()) {
			WINDOW_STATE.set_status_label(L"Waiting until the game exits...");
		}
		log_printf("'run_at_exit' setting is set. Running %s...\n", exe_fn);

		std::wstring pid_wstr = std::to_wstring(runconfig_loader_pid_get());
		HANDLE hSharedMem = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 128 * sizeof(DWORD), (L"thcrap loader process list "sv + pid_wstr).c_str());
		DWORD* pid_list = (DWORD*)MapViewOfFile(hSharedMem, FILE_MAP_ALL_ACCESS, 0, 0, 128 * sizeof(DWORD));

		HANDLE hMutex = CreateMutexW(nullptr, false, (L"thcrap loader process list mutex "sv + pid_wstr).c_str());

		HANDLE hProcess1;
		ret = thcrap_inject_into_new(exe_fn, args, &hProcess1, NULL);
		log_printf("Waiting until %s is finished... ", exe_fn);
		WaitForSingleObject(hProcess1, INFINITE);
		CloseHandle(hProcess1);

		std::vector<HANDLE> processHandleList;
		size_t handle_list_size;
		for (;;) {
			for (auto i : processHandleList) {
				CloseHandle(i);
			}
			processHandleList = {};

			WaitForSingleObject(hMutex, INFINITE);
			for (size_t i = 0; i < 128; i++) {
				if (pid_list[i]) {
					HANDLE hNewProcess = OpenProcess(SYNCHRONIZE, FALSE, pid_list[i]);
					if (hNewProcess) {
						processHandleList.push_back(hNewProcess);
					}
					else {
						pid_list[i] = 0;
					}
				}
			}
			ReleaseMutex(hMutex);

			handle_list_size = processHandleList.size();
			if (!handle_list_size) {
				break;
			}
			WaitForMultipleObjects(handle_list_size, processHandleList.data(), false, INFINITE);
		}

		CloseHandle(hMutex);
		UnmapViewOfFile(pid_list);
		CloseHandle(hSharedMem);
		SetEvent(UPDATE_STATE.event_update_wrapper_terminate);

		log_print("done.\n");
	}

	// Update the thcrap engine
	log_print("Looking for thcrap updates...\n");

	DWORD cur_dir_len = GetCurrentDirectoryU(0, nullptr);
	VLA(char, cur_dir, cur_dir_len);
	GetCurrentDirectoryU(cur_dir_len, cur_dir);
	runconfig_thcrap_dir_set(cur_dir);
	VLA_FREE(cur_dir);

	if (update_notify_thcrap() == SELF_OK && !UPDATE_STATE.game_started) {
		// Re-run an up-to-date loader
		LPSTR commandLine = GetCommandLineU();
		log_printf("Update found! Re-running %s\n", commandLine);

		STARTUPINFOA sa = {};
		PROCESS_INFORMATION pi = {};
		sa.cb = sizeof(sa);
		CreateProcessU(NULL, commandLine, NULL, NULL, FALSE, 0, NULL, NULL, &sa, &pi);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		goto end;
	}
	log_print(
		"thcrap is up to date.\n"
		"Updating patches with global filter (don't download game-specific files)...\n"
	);
	loader_update_progress_init(STATE_CORE_UPDATE);
	stack_update(update_filter_global, NULL, loader_update_progress_callback, NULL);
	log_print("Stack update done.\n");

	const char* game = runconfig_game_get();
	if (!game) {
		game = game_id_other;
	}
	if (game) {
		log_print("Updating patches with game-specific filter...\n");
		if (WINDOW_STATE.enabled()) {
			WINDOW_STATE.set_control_enabled(RUN_BUTTON, true);
		}
		loader_update_progress_init(STATE_PATCHES_UPDATE);
		const char *filter[] = { game, NULL };
		stack_update(update_filter_games, filter, loader_update_progress_callback, NULL);
		log_print("Stack update done.\n");
	}

	bool game_started = UPDATE_STATE.game_started.exchange(true);

	if (!game_started) {
		log_printf("Starting %s with arguments %s... ", exe_fn, args);
		ret = thcrap_inject_into_new(exe_fn, args, NULL, NULL);
		log_print("done.\n");
	}

	if (
		!(UPDATE_STATE.update_at_exit && !game) &&
		(UPDATE_STATE.background_updates || !game)
	) {
		DWORD time_between_updates = INFINITE;
		HANDLE handles[] = {
			UPDATE_STATE.event_wrapper_request_update,
			WINDOW_STATE.hThread,
			WINDOW_STATE.event_request_update
		};

		for (;;) {
			if (WINDOW_STATE.enabled()) {
				WINDOW_STATE.progress_bar_set_marquee<true>();
				WINDOW_STATE.progress_bar_clear_text();
				WINDOW_STATE.set_control_enabled(UPDATE_BUTTON, false);
			}
			if (UPDATE_STATE.background_updates && (game || game_id_other)) {
				if (UPDATE_STATE.update_others) {
					log_print("Updating all patches with no filter...\n");
					loader_update_progress_init(STATE_GLOBAL_UPDATE);
					global_update(loader_update_progress_callback, NULL);
					log_print("Global update done.\n");
				}
				else if (game) {
					log_print("Updating patches with game-specific filter...\n");
					loader_update_progress_init(STATE_PATCHES_UPDATE);
					const char* filter[] = { game, NULL };
					stack_update(update_filter_games, filter, loader_update_progress_callback, NULL);
					log_print("Stack update done.\n");
				}
				UPDATE_STATE.state = STATE_WAITING;
				// Display the "Update finished" message
				if (WINDOW_STATE.enabled()) {
					WINDOW_STATE.set_control_enabled(UPDATE_BUTTON, true);
					WINDOW_STATE.set_status_label(L"Update finished");
					WINDOW_STATE.progress_bar_set_marquee<false>();
					WINDOW_STATE.progress_bar_clear_text();
					WINDOW_STATE.progress_bar_set_percent(100);
				}

				unsigned int update_delay = UPDATE_STATE.time_between_updates;
				time_between_updates = update_delay * 60 * 1000;
				// Wait until the next update
				log_printf("Update finished. Waiting until next update (%u min)... ", update_delay);
			}

			if (!game && !game_id_other) {
				if (WINDOW_STATE.enabled()) {
					WINDOW_STATE.set_status_label(L"Waiting for game to open");
				}
			}

			switch (WaitForMultipleObjects(elementsof(handles), handles, FALSE, time_between_updates)) {
				case WAIT_TIMEOUT:
					if (UPDATE_STATE.background_updates) {
						log_print("timeout, running update.\n");
					}
					continue;
				case (WAIT_OBJECT_0 + 2):
					if (UPDATE_STATE.background_updates) {
						log_print("update button clicked, running update.\n");
					}
					continue;
				case (WAIT_OBJECT_0 + 0): {
					log_print(
						"Update requested by wrapper patch\n"
						"Updating patches with game-specific filter...\n"
					);
					loader_update_progress_init(STATE_PATCHES_UPDATE);
					const char* filter[] = { game_id_other, NULL };
					stack_update(update_filter_games, filter, loader_update_progress_callback, NULL);
					log_print("Stack update done.\n");
					SetEvent(UPDATE_STATE.event_update_finished);
					continue;
				}
			}
			log_print("main window closed. Exiting.\n");
			break;
		}
	}
	else {
		log_print("Background updates are disabled. Closing thcrap_loader.\n");
		WINDOW_STATE.close();
	}
	WaitForSingleObject(hWrapperUpdateThread, INFINITE);
	CloseHandle(hWrapperUpdateThread);

end:
	
	CloseHandle(WINDOW_STATE.hThread);
	CloseHandle(WINDOW_STATE.event_request_update);
	CloseHandle(UPDATE_STATE.event_wrapper_request_update);
	CloseHandle(UPDATE_STATE.event_update_wrapper_terminate);
	CloseHandle(UPDATE_STATE.event_update_finished);
	CloseHandle(UPDATE_MUTEX);
	return ret;
}

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD ulReasonForCall, LPVOID lpReserved) {
	switch (ulReasonForCall) {
		case DLL_PROCESS_ATTACH:
			updater_module = (HMODULE)hDll;
			DisableThreadLibraryCalls(hDll);
			break;
	}
	return TRUE;
}
