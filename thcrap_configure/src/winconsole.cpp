#include <thcrap.h>
#include <windows.h>
#include <windowsx.h>
#include "console.h"
#include "dialog.h"
#include "resource.h"
#include <string.h>
#include <CommCtrl.h>
#include <queue>
#include <mutex>
#include <future>
#include <memory>
#include <thread>

template<typename T>
using EventPtr = const std::shared_ptr<std::promise<T>>*;

struct ConsoleDialog : Dialog<ConsoleDialog> {
private:
	friend Dialog<ConsoleDialog>;
	HINSTANCE getInstance();
	LPCWSTR getTemplate();
	INT_PTR dialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
private:
	HWND buttonNext = NULL;
	HWND edit = NULL;
	HWND list = NULL;
	HWND progress = NULL;
	HWND staticText = NULL;
	HWND buttonYes = NULL;
	HWND buttonNo = NULL;

	enum Mode {
		MODE_INPUT = 0,
		MODE_PAUSE,
		MODE_PROGRESS_BAR,
		MODE_NONE,
		MODE_ASK_YN,
	};
	int currentMode = -1;
	void setMode(Mode mode);

	static LRESULT CALLBACK editProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK listProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:
	EventPtr<void> onInit;
};
static ConsoleDialog g_console_xxx{};
static ConsoleDialog *g_console = &g_console_xxx;


#define APP_READQUEUE (WM_APP+0)
/* lparam = const std::shared_ptr<std::promise<char>>* */
#define APP_ASKYN (WM_APP+1)
/* wparam = length of output string */
/* lparam = const std::shared_ptr<std::promise<wchar_t*>>* */
#define APP_GETINPUT (WM_APP+2)
/* wparam = WM_CHAR */
#define APP_LISTCHAR (WM_APP+3)
#define APP_UPDATE (WM_APP+4)
#define APP_PREUPDATE (WM_APP+5)
/* lparam = const std::shared_ptr<std::promise<void>>* */
#define APP_PAUSE (WM_APP+6)
/* wparam = percent */
#define APP_PROGRESS (WM_APP+7)

/* Reads line queue */
#define Thcrap_ReadQueue(hwndDlg) ((void)::PostMessage((hwndDlg), APP_READQUEUE, 0, 0L))
#define Thcrap_Askyn(hwndDlg,a_promise) ((void)::PostMessage((hwndDlg), APP_ASKYN, 0, (LPARAM)(EventPtr<char>)(a_promise)))
/* Request input */
/* UI thread will signal g_event when user confirms input */
#define Thcrap_GetInput(hwndDlg,len,a_promise) ((void)::PostMessage((hwndDlg), APP_GETINPUT, (WPARAM)(DWORD)(len), (LPARAM)(EventPtr<wchar_t*>)(a_promise)))
/* Should be called after adding/appending bunch of lines */
#define Thcrap_Update(hwndDlg) ((void)::PostMessage((hwndDlg), APP_UPDATE, 0, 0L))
/* Should be called before adding lines*/
#define Thcrap_Preupdate(hwndDlg) ((void)::PostMessage((hwndDlg), APP_PREUPDATE, 0, 0L))
/* Pause */
#define Thcrap_Pause(hwndDlg,a_promise) ((void)::PostMessage((hwndDlg), APP_PAUSE, 0, (LPARAM)(EventPtr<void>)(a_promise)))
/* Updates progress bar */
#define Thcrap_Progress(hwndDlg, pc) ((void)::PostMessage((hwndDlg), APP_PROGRESS, (WPARAM)(DWORD)(pc), 0L))

enum LineType {
	LINE_ADD,
	LINE_APPEND,
	LINE_CLS,
	LINE_PENDING
};
struct LineEntry {
	LineType type;
	std::wstring content;
};
template<typename T>
class WaitForEvent {
	std::shared_ptr<std::promise<T>> promise;
	std::future<T> future;
public:
	WaitForEvent() {
		promise = std::make_shared<std::promise<T>>();
		future = promise->get_future();
	}
	EventPtr<T> getEvent() {
		return &promise;
	}
	T getResponse() {
		return future.get();
	}
};
template<typename T>
void SignalEvent(std::shared_ptr<std::promise<T>>& ptr, T val) {
	if (ptr) {
		ptr->set_value(val);
		ptr.reset();
	}
}
void SignalEvent(std::shared_ptr<std::promise<void>>& ptr) {
	if (ptr) {
		ptr->set_value();
		ptr.reset();
	}
}

static HWND g_hwnd = NULL;
static std::mutex g_mutex; // used for synchronizing the queue
static std::queue<LineEntry> g_queue;
static std::vector<std::wstring> q_responses;
static std::shared_ptr<std::promise<void>> g_exitguithreadevent;
void ConsoleDialog::setMode(Mode mode) {
	if (currentMode == mode)
		return;
	currentMode = mode;
	static const HWND ConsoleDialog::*items[6] = {
		&ConsoleDialog::buttonNext,
		&ConsoleDialog::edit,
		&ConsoleDialog::progress,
		&ConsoleDialog::staticText,
		&ConsoleDialog::buttonYes,
		&ConsoleDialog::buttonNo
	};

	static const bool modes[5][6] = {
		{ true, true, false, false, false, false }, // MODE_INPUT
		{ true, false, false, false, false, false }, // MODE_PAUSE
		{ false, false, true, false, false, false }, // MODE_PROGRESS_BAR
		{ false, false, false, true, false, false }, // MODE_NONE
		{ false, false, false, false, true, true }, // MODE_ASK_YN
	};
	for (int i = 0; i < 6; i++) {
		if (modes[mode][i]) {
			EnableWindow(this->*items[i], TRUE);
			ShowWindow(this->*items[i], SW_SHOW);
		} else {
			ShowWindow(this->*items[i], SW_HIDE);
			EnableWindow(this->*items[i], FALSE);
		}
	}
}

static WNDPROC getClassWindowProc(HINSTANCE hInstance, LPCWSTR lpClassName) {
	WNDCLASS wndClass;
	if (!GetClassInfoW(hInstance, lpClassName, &wndClass))
		assert(0);
	return wndClass.lpfnWndProc;
}

LRESULT CALLBACK ConsoleDialog::editProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	ConsoleDialog *dlg = fromHandle(GetParent(hWnd));
	static WNDPROC origEditProc = NULL;
	if (!origEditProc)
		origEditProc = getClassWindowProc(NULL, WC_EDITW);

	if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
		SendMessage(dlg->hWnd, WM_COMMAND, MAKELONG(IDC_BUTTON1, BN_CLICKED), (LPARAM)hWnd);
		return 0;
	} else if (uMsg == WM_GETDLGCODE && wParam == VK_RETURN) {
		// nescessary for control to recieve VK_RETURN
		return CallWindowProcW(origEditProc, hWnd, uMsg, wParam, lParam) | DLGC_WANTALLKEYS;
	} else if (uMsg == WM_KEYDOWN && (wParam == VK_UP || wParam == VK_DOWN)) {
		// Switch focus to list on up/down arrows
		SetFocus(dlg->list);
		return 0;
	} else if ((uMsg == WM_KEYDOWN || uMsg == WM_KEYUP) && (wParam == VK_PRIOR || wParam == VK_NEXT)) {
		// Switch focus to list on up/down arrows
		SendMessage(dlg->list, uMsg, wParam, lParam);
		return 0;
	}
	return CallWindowProcW(origEditProc, hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ConsoleDialog::listProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	ConsoleDialog *dlg = fromHandle(GetParent(hWnd));
	static WNDPROC origListProc = NULL;
	if (!origListProc)
		origListProc = getClassWindowProc(NULL, WC_LISTBOXW);

	if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
		// if edit already has something in it, clicking on the button
		// otherwise doubleclick the list
		int len = GetWindowTextLength(dlg->edit);
		SendMessage(dlg->hWnd, WM_COMMAND, len ? MAKELONG(IDC_BUTTON1, BN_CLICKED) : MAKELONG(IDC_LIST1, LBN_DBLCLK), (LPARAM)hWnd);
		return 0;
	} else if (uMsg == WM_GETDLGCODE && wParam == VK_RETURN) {
		// nescessary for control to recieve VK_RETURN
		return CallWindowProcW(origListProc, hWnd, uMsg, wParam, lParam) | DLGC_WANTALLKEYS;
	} else if (uMsg == WM_CHAR) {
		// let parent dialog process the keypresses from list
		SendMessage(dlg->hWnd, APP_LISTCHAR, wParam, lParam);
		return 0;
	}
	return CallWindowProcW(origListProc, hWnd, uMsg, wParam, lParam);
}

bool con_can_close = false;
HINSTANCE ConsoleDialog::getInstance() {
	return GetModuleHandle(NULL);
}
LPCWSTR ConsoleDialog::getTemplate() {
	return MAKEINTRESOURCEW(IDD_DIALOG1);
}
INT_PTR ConsoleDialog::dialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static int input_len = 0;
	static int last_index = 0;
	static std::wstring pending = L"";
	static std::shared_ptr<std::promise<char>> promiseyn; // APP_ASKYN event
	static std::shared_ptr<std::promise<wchar_t*>> promise; // APP_GETINPUT event
	static std::shared_ptr<std::promise<void>> promisev; // APP_PAUSE event
	static HICON hIconSm = NULL, hIcon = NULL;
	switch (uMsg) {
	case WM_INITDIALOG: {
		g_hwnd = hWnd;
		buttonNext = GetDlgItem(hWnd, IDC_BUTTON1);
		edit = GetDlgItem(hWnd, IDC_EDIT1);
		list = GetDlgItem(hWnd, IDC_LIST1);
		progress = GetDlgItem(hWnd, IDC_PROGRESS1);
		staticText = GetDlgItem(hWnd, IDC_STATIC1);
		buttonYes = GetDlgItem(hWnd, IDC_BUTTON_YES);
		buttonNo = GetDlgItem(hWnd, IDC_BUTTON_NO);

		SendMessage(progress, PBM_SETRANGE, 0, MAKELONG(0, 100));
		SetWindowLongPtr(edit, GWLP_WNDPROC, (LONG_PTR)editProc);
		SetWindowLongPtr(list, GWLP_WNDPROC, (LONG_PTR)listProc);

		// set icon
		hIconSm = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON,
			GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
		hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON,
			GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
		if (hIconSm) {
			SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);
		}
		if (hIcon) {
			SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
		}
		setMode(MODE_NONE);

		RECT workarea, winrect;
		SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&workarea, 0);
		GetWindowRect(hWnd, &winrect);
		int width = winrect.right - winrect.left;
		MoveWindow(hWnd,
			workarea.left + ((workarea.right - workarea.left) / 2) - (width / 2),
			workarea.top,
			width,
			workarea.bottom - workarea.top,
			TRUE);

		(*onInit)->set_value();
		return TRUE;
	}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BUTTON1:
			if (currentMode == MODE_INPUT) {
				wchar_t* input_str = new wchar_t[input_len];
				GetWindowTextW(edit, input_str, input_len);
				SetWindowTextW(edit, L"");
				setMode(MODE_NONE);
				SignalEvent(promise, input_str);
			}
			else if (currentMode == MODE_PAUSE) {
				setMode(MODE_NONE);
				SignalEvent(promisev);
			}
			return TRUE;
		case IDC_BUTTON_YES:
		case IDC_BUTTON_NO:
			if (currentMode == MODE_ASK_YN) {
				setMode(MODE_NONE);
				SignalEvent(promiseyn, LOWORD(wParam) == IDC_BUTTON_YES ? 'y' : 'n');
			}
			return TRUE;
		case IDC_LIST1:
			switch (HIWORD(wParam)) {
			case LBN_DBLCLK: {
				int cur = ListBox_GetCurSel((HWND)lParam);
				if (currentMode == MODE_INPUT && cur != LB_ERR && (!q_responses[cur].empty() || cur == last_index)) {
					wchar_t* input_str = new wchar_t[q_responses[cur].length() + 1];
					wcscpy(input_str, q_responses[cur].c_str());
					SetDlgItemTextW(hWnd, IDC_EDIT1, L"");
					setMode(MODE_NONE);
					SignalEvent(promise, input_str);
				}
				else if (currentMode == MODE_PAUSE) {
					setMode(MODE_NONE);
					SignalEvent(promisev);
				}
				return TRUE;
			}
			}
		}
		return FALSE;
	case APP_LISTCHAR:
		if (currentMode == MODE_INPUT) {
			SetFocus(edit);
			SendMessage(edit, WM_CHAR, wParam, lParam);
		}
		else if (currentMode == MODE_ASK_YN) {
			char c = wctob(towlower(wParam));
			if (c == 'y' || c == 'n') {
				setMode(MODE_ASK_YN);
				SignalEvent(promiseyn, c);
			}
		}
		return TRUE;
	case APP_READQUEUE: {
		std::lock_guard<std::mutex> lock(g_mutex);
		while (!g_queue.empty()) {
			LineEntry& ent = g_queue.front();
			switch (ent.type) {
			case LINE_ADD:
				last_index = ListBox_AddString(list, ent.content.c_str());
				q_responses.push_back(pending);
				pending = L"";
				break;
			case LINE_APPEND: {
				int origlen = ListBox_GetTextLen(list, last_index);
				int len = origlen + ent.content.length() + 1;
				VLA(wchar_t, wstr, len);
				ListBox_GetText(list, last_index, wstr);
				wcscat_s(wstr, len, ent.content.c_str());
				ListBox_DeleteString(list, last_index);
				ListBox_InsertString(list, last_index, wstr);
				VLA_FREE(wstr);
				if (!pending.empty()) {
					q_responses[last_index] = pending;
					pending = L"";
				}
				break;
			}
			case LINE_CLS:
				ListBox_ResetContent(list);
				q_responses.clear();
				pending = L"";
				break;
			case LINE_PENDING:
				pending = ent.content;
				break;
			}
			g_queue.pop();
		}
		return TRUE;
	}
	case APP_GETINPUT:
		setMode(MODE_INPUT);
		input_len = wParam;
		promise = *(EventPtr<wchar_t*>)lParam;
		return TRUE;
	case APP_PAUSE:
		setMode(MODE_PAUSE);
		promisev = *(EventPtr<void>)lParam;
		return TRUE;
	case APP_ASKYN:
		setMode(MODE_ASK_YN);
		promiseyn = *(EventPtr<char>)lParam;
		return TRUE;
	case APP_PREUPDATE:
		SetWindowRedraw(list, FALSE);
		return TRUE;
	case APP_UPDATE:
		//SendMessage(list, WM_VSCROLL, SB_BOTTOM, 0L);
		ListBox_SetCurSel(list, last_index);// Just scrolling doesn't work well
		SetWindowRedraw(list, TRUE);
		// this causes unnescessary flickering
		//RedrawWindow(list, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
		return TRUE;
	case APP_PROGRESS:
		if (wParam == -1) {
			SetWindowLong(progress, GWL_STYLE, GetWindowLong(progress, GWL_STYLE) | PBS_MARQUEE);
			SendMessage(progress, PBM_SETMARQUEE, 1, 0L);
		}
		else {
			SendMessage(progress, PBM_SETMARQUEE, 0, 0L);
			SetWindowLong(progress, GWL_STYLE, GetWindowLong(progress, GWL_STYLE) & ~PBS_MARQUEE);
			SendMessage(progress, PBM_SETPOS, wParam, 0L);
		}

		setMode(MODE_PROGRESS_BAR);
		return TRUE;
	case WM_SIZE: {
		RECT baseunits;
		baseunits.left = baseunits.right = 100;
		baseunits.top = baseunits.bottom = 100;
		MapDialogRect(hWnd, &baseunits);
		int basex = 4 * baseunits.right / 100;
		int basey = 8 * baseunits.bottom / 100;
		long origright = LOWORD(lParam) * 4 / basex;
		long origbottom = HIWORD(lParam) * 8 / basey;
		RECT rbutton1, rbutton2, rlist, redit, rwide;
		const int MARGIN = 7;
		const int PADDING = 2;
		const int BTN_WIDTH = 50;
		const int BTN_HEIGHT = 14;
		// |-----------wide----------|
		// |-----edit-------|        |
		// |       |--btn2--|--btn1--|

		// Button size
		rbutton1 = {
			origright - MARGIN - BTN_WIDTH,
			origbottom - MARGIN - BTN_HEIGHT,
			origright - MARGIN,
			origbottom - MARGIN };
		MapDialogRect(hWnd, &rbutton1);
		// Progress/staic size
		rwide = {
			MARGIN,
			origbottom - MARGIN - BTN_HEIGHT,
			origright - MARGIN,
			origbottom - MARGIN };
		MapDialogRect(hWnd, &rwide);
		// Edit size
		redit = {
			MARGIN,
			origbottom - MARGIN - BTN_HEIGHT,
			origright - MARGIN - BTN_WIDTH - PADDING,
			origbottom - MARGIN };
		MapDialogRect(hWnd, &redit);
		// Button2 size
		rbutton2 = {
			origright - MARGIN - BTN_WIDTH - PADDING - BTN_WIDTH,
			origbottom - MARGIN - BTN_HEIGHT,
			origright - MARGIN - BTN_WIDTH - PADDING,
			origbottom - MARGIN };
		MapDialogRect(hWnd, &rbutton2);
		// List size
		rlist = {
			MARGIN,
			MARGIN,
			origright - MARGIN,
			origbottom - MARGIN - BTN_HEIGHT - PADDING};
		MapDialogRect(hWnd, &rlist);
#define MoveToRect(hwnd,rect) MoveWindow((hwnd), (rect).left, (rect).top, (rect).right - (rect).left, (rect).bottom - (rect).top, TRUE)
		MoveToRect(buttonNext, rbutton1);
		MoveToRect(edit, redit);
		MoveToRect(list, rlist);
		MoveToRect(progress, rwide);
		MoveToRect(staticText, rwide);
		MoveToRect(buttonYes, rbutton2);
		MoveToRect(buttonNo, rbutton1);
		InvalidateRect(hWnd, &rwide, FALSE); // needed because static doesn't repaint on move
		return TRUE;
	}
	case WM_CLOSE:
		if (con_can_close || MessageBoxW(hWnd, L"Patch configuration is not finished.\n\nQuit anyway?", L"Touhou Community Reliant Automatic Patcher", MB_YESNO | MB_ICONWARNING) == IDYES) {
			EndDialog(hWnd, 0);
		}
		return TRUE;
	case WM_NCDESTROY:
		g_hwnd = NULL;
		if (hIconSm) {
			DestroyIcon(hIconSm);
		}
		if (hIcon) {
			DestroyIcon(hIcon);
		}
		return TRUE;
	}
	return FALSE;
}
static bool needAppend = false;
static bool dontUpdate = false;
void log_windows(const char* text) {
	if (!g_hwnd) return;
	if (!dontUpdate) Thcrap_Preupdate(g_hwnd);

	int len = strlen(text) + 1;
	VLA(wchar_t, wtext, len);
	StringToUTF16(wtext, text, len);
	wchar_t *start = wtext, *end = wtext;
	while (end) {
		int mutexcond = 0;
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			end = wcschr(end, '\n');
			if (!end) end = wcschr(start, '\0');
			wchar_t c = *end++;
			if (c == '\n') {
				end[-1] = '\0'; // Replace newline with null
			}
			else if (c == '\0') {
				if (end != wtext && end == start) {
					break; // '\0' right after '\n'
				}
				end = NULL;
			}
			else continue;
			if (needAppend == true) {
				LineEntry le = { LINE_APPEND, start };
				g_queue.push(le);
				needAppend = false;
			}
			else {
				LineEntry le = { LINE_ADD, start };
				g_queue.push(le);
			}
			mutexcond = g_queue.size() > 10;
		}
		if (mutexcond) {
			Thcrap_ReadQueue(g_hwnd);
			if (dontUpdate) {
				Thcrap_Update(g_hwnd);
				Thcrap_Preupdate(g_hwnd);
			}
		}
		start = end;
	}
	VLA_FREE(wtext);
	if (!end) needAppend = true;

	if (!dontUpdate) {
		Thcrap_ReadQueue(g_hwnd);
		Thcrap_Update(g_hwnd);
	}
}
void log_nwindows(const char* text, size_t len) {
	VLA(char, ntext, len+1);
	memcpy(ntext, text, len);
	ntext[len] = '\0';
	log_windows(ntext);
	VLA_FREE(ntext);
}
/* --- code proudly stolen from thcrap/log.cpp --- */
#define VLA_VSPRINTF(str, va) \
	size_t str##_full_len = _vscprintf(str, va) + 1; \
	VLA(char, str##_full, str##_full_len); \
	/* vs*n*printf is not available in msvcrt.dll. Doesn't matter anyway. */ \
	vsprintf(str##_full, str, va);

void con_vprintf(const char *str, va_list va)
{
	if (str) {
		VLA_VSPRINTF(str, va);
		log_windows(str_full);
		//printf("%s", str_full);
		VLA_FREE(str_full);
	}
}

void con_printf(const char *str, ...)
{
	if (str) {
		va_list va;
		va_start(va, str);
		con_vprintf(str, va);
		va_end(va);
	}
}
/* ------------------- */
void con_clickable(const char* response) {
	int len = strlen(response) + 1;
	VLA(wchar_t, wresponse, len);
	StringToUTF16(wresponse, response, len);
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		LineEntry le = { LINE_PENDING,  wresponse };
		g_queue.push(le);
	}
	VLA_FREE(wresponse);
}

void con_clickable(int response) {
	size_t response_full_len = _scprintf("%d", response) + 1;
	VLA(char, response_full, response_full_len);
	sprintf(response_full, "%d", response);
	con_clickable(response_full);
}
void console_init() {
	INITCOMMONCONTROLSEX iccex;
	iccex.dwSize = sizeof(iccex);
	iccex.dwICC = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&iccex);

	log_set_hook(log_windows, log_nwindows);

	WaitForEvent<void> e;
	std::thread([](EventPtr<void> onInit) {
		g_console->onInit = onInit;
		g_console->createModal(NULL);
		log_set_hook(NULL, NULL);
		if (!g_exitguithreadevent) {
			exit(0); // Exit the entire process when exiting this thread
		}
		else {
			SignalEvent(g_exitguithreadevent);
		}
	}, e.getEvent()).detach();
	e.getResponse();
}
char* console_read(char *str, int n) {
	dontUpdate = false;
	Thcrap_ReadQueue(g_hwnd);
	Thcrap_Update(g_hwnd);
	WaitForEvent<wchar_t*> e;
	Thcrap_GetInput(g_hwnd, n, e.getEvent());
	wchar_t* input = e.getResponse();
	StringToUTF8(str, input, n);
	delete[] input;
	needAppend = false; // gotta insert that newline
	return str;
}
void cls(SHORT top) {
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		LineEntry le = { LINE_CLS, L"" };
		g_queue.push(le);
		Thcrap_ReadQueue(g_hwnd);
		needAppend = false;
	}
	if (dontUpdate) {
		Thcrap_Update(g_hwnd);
		Thcrap_Preupdate(g_hwnd);
	}
}
void pause(void) {
	dontUpdate = false;
	con_printf("Press ENTER to continue . . . "); // this will ReadQueue and Update for us
	needAppend = false;
	WaitForEvent<void> e;
	Thcrap_Pause(g_hwnd, e.getEvent());
	e.getResponse();
}
void console_prepare_prompt(void) {
	Thcrap_Preupdate(g_hwnd);
	dontUpdate = true;
}
void console_print_percent(int pc) {
	Thcrap_Progress(g_hwnd, pc);
}
char console_ask_yn(const char* what) {
	dontUpdate = false;
	log_windows(what);
	needAppend = false;

	WaitForEvent<char> e;
	Thcrap_Askyn(g_hwnd, e.getEvent());
	return e.getResponse();
}
void con_end(void) {
	WaitForEvent<void> e;
	g_exitguithreadevent = *e.getEvent();
	SendMessage(g_hwnd, WM_CLOSE, 0, 0L);
	e.getResponse();
}
HWND con_hwnd(void) {
	return g_hwnd;
}
int console_width(void) {
	return 80;
}
