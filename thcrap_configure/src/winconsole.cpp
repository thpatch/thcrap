#include <thcrap.h>
#include <windows.h>
#include <windowsx.h>
#define ProgressBar_SetRange(hwndCtl, low, high) ((DWORD)SNDMSG((hwndCtl), PBM_SETRANGE, 0L, MAKELPARAM((low), (high))))
#define ProgressBar_SetPos(hwndCtl, pos) ((int)(DWORD)SNDMSG((hwndCtl), PBM_SETPOS, (WPARAM)(int)(pos), 0L))
#define ProgressBar_SetMarquee(hwndCtl, fEnable, animTime) ((BOOL)(DWORD)SNDMSG((hwndCtl), PBM_SETMARQUEE, (WPARAM)(BOOL)(fEnable), (LPARAM)(DWORD)(animTime)))
#include "configure.h"
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
#include <utility>
#include <type_traits>
#include <string_view>

// A wrapper around std::promise that lets us call set_value without
// having to worry about promise_already_satisfied/no_state.
template<typename R>
class PromiseSlot {
	static inline constexpr bool is_void = std::is_void<R>::value;
	static inline constexpr bool is_ref = std::is_lvalue_reference<R>::value;

	std::promise<R> promise;
	bool present;
public:
	PromiseSlot() {
		release();
	}
	PromiseSlot(std::promise<R> &&other) :promise(std::move(other)), present(true) {
	}
	PromiseSlot &operator=(std::promise<R> &&other) {
		std::promise<R>{std::move(other)}.swap(promise);
		present = true;
		return *this;
	}
	std::promise<R> release() {
		present = false;
		return std::promise<R>{std::move(promise)};
	}
	template<typename RR = std::enable_if_t<!is_void && !is_ref, R>>
	void set_value(RR &&value) {
		if (present)
			release().set_value(std::move(value));
	}
	template<typename RR = std::enable_if_t<!is_void && !is_ref, R>>
	void set_value(const RR &value) {
		if (present)
			release().set_value(value);
	}
	template<typename = std::enable_if_t<is_void>>
	void set_value() {
		if (present)
			release().set_value();
	}
	explicit operator bool() {
		return present;
	}
};

enum LineType {
	LINE_ADD,
	LINE_APPEND,
	LINE_CLS
};
struct LineEntry {
	LineType type;
	std::wstring content;
	std::wstring response;
};

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

	void setMarquee(BOOL fEnable);

	PromiseSlot<char> onYesNo; // APP_ASKYN event
	PromiseSlot<std::wstring> onInput; // APP_GETINPUT event
	PromiseSlot<void> onUnpause; // APP_PAUSE event

	enum {
		APP_READQUEUE = WM_APP,
		APP_ASKYN, // lparam = std::promise<char>*
		APP_GETINPUT, // lparam = std::promise<wstring>*
		APP_UPDATE,
		APP_PREUPDATE,
		APP_PAUSE, // lparam = std::promise<void>*
		APP_PROGRESS, // wparam = percent
	};

	std::mutex mutex; // used for synchronizing the queue
	std::queue<LineEntry> queue;
	std::vector<std::wstring> responses;

	int last_index = 0;
	std::wstring last_line = L"";
	HICON hIconSm = NULL;
	HICON hIcon = NULL;

	bool popQueue(LineEntry &ent);
public:
	PromiseSlot<void> onInit;

	void readQueue();
	std::future<char> askyn();
	std::future<std::wstring> getInput();
	void update();
	void preupdate();
	std::future<void> pause();
	void setProgress(int pc);

	void pushQueue(LineEntry &&ent);
};
static ConsoleDialog g_console_xxx{};
static ConsoleDialog *g_console = &g_console_xxx;

void ConsoleDialog::readQueue() {
	PostMessage(hWnd, APP_READQUEUE, 0, 0L);
}
std::future<char> ConsoleDialog::askyn() {
	std::promise<char> promise;
	std::future<char> future = promise.get_future();
	SendMessage(hWnd, APP_ASKYN, 0, (LPARAM)&promise);
	return future;
}
std::future<std::wstring> ConsoleDialog::getInput() {
	std::promise<std::wstring> promise;
	std::future<std::wstring> future = promise.get_future();
	SendMessage(hWnd, APP_GETINPUT, 0, (LPARAM)&promise);
	return future;
}
// Should be called after adding/appending bunch of lines
void ConsoleDialog::update() {
	PostMessage(hWnd, APP_UPDATE, 0, 0L);
}
// Should be called before adding lines
void ConsoleDialog::preupdate() {
	PostMessage(hWnd, APP_PREUPDATE, 0, 0L);
}
std::future<void> ConsoleDialog::pause() {
	std::promise<void> promise;
	std::future<void> future = promise.get_future();
	SendMessage(hWnd, APP_PAUSE, 0, (LPARAM)&promise);
	return future;
}
void ConsoleDialog::setProgress(int pc) {
	PostMessage(hWnd, APP_PROGRESS, (WPARAM)(DWORD)pc, 0L);
}

bool ConsoleDialog::popQueue(LineEntry &ent) {
	if (!queue.size())
		return false;

	ent = std::move(queue.front());
	queue.pop();
	return true;
}
void ConsoleDialog::pushQueue(LineEntry &&ent) {
	std::lock_guard<std::mutex> lock(mutex);
	queue.push(std::move(ent));
	if (queue.size() > 10)
		readQueue();
}

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
		SendMessage(dlg->hWnd, WM_COMMAND, MAKEWPARAM(IDC_BUTTON1, BN_CLICKED), (LPARAM)dlg->buttonNext);
		return 0;
	} else if (uMsg == WM_CHAR && wParam == L'\r') {
		// ignoring this message prevents a beep
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
		if (dlg->currentMode == MODE_INPUT) {
			SetFocus(dlg->edit);
			SendMessage(dlg->edit, WM_CHAR, wParam, lParam);
		} else if (dlg->currentMode == MODE_ASK_YN) {
			if (wParam == L'Y' || wParam == L'y')
				SendMessage(dlg->hWnd, WM_COMMAND, MAKEWPARAM(IDC_BUTTON_YES, BN_CLICKED), (LPARAM)dlg->buttonYes);
			else if (wParam == L'N' || wParam == L'n')
				SendMessage(dlg->hWnd, WM_COMMAND, MAKEWPARAM(IDC_BUTTON_NO, BN_CLICKED), (LPARAM)dlg->buttonNo);
		}
		return 0;
	}
	return CallWindowProcW(origListProc, hWnd, uMsg, wParam, lParam);
}

void ConsoleDialog::setMarquee(BOOL fEnable) {
	DWORD dwStyle = GetWindowLong(progress, GWL_STYLE);
	if (!!(dwStyle & PBS_MARQUEE) != fEnable) {
		if (fEnable)
			dwStyle |= PBS_MARQUEE;
		else
			dwStyle &= ~PBS_MARQUEE;
		SetWindowLong(progress, GWL_STYLE, dwStyle);
		ProgressBar_SetMarquee(progress, fEnable, 0L);
	}
}

bool con_can_close = false;
HINSTANCE ConsoleDialog::getInstance() {
	return GetModuleHandle(NULL);
}
LPCWSTR ConsoleDialog::getTemplate() {
	return MAKEINTRESOURCEW(IDD_DIALOG1);
}
INT_PTR ConsoleDialog::dialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG: {
		buttonNext = GetDlgItem(hWnd, IDC_BUTTON1);
		edit = GetDlgItem(hWnd, IDC_EDIT1);
		list = GetDlgItem(hWnd, IDC_LIST1);
		progress = GetDlgItem(hWnd, IDC_PROGRESS1);
		staticText = GetDlgItem(hWnd, IDC_STATIC1);
		buttonYes = GetDlgItem(hWnd, IDC_BUTTON_YES);
		buttonNo = GetDlgItem(hWnd, IDC_BUTTON_NO);

		ProgressBar_SetRange(progress, 0, 100);
		SetWindowLongPtr(edit, GWLP_WNDPROC, (LONG_PTR)editProc);
		SetWindowLongPtr(list, GWLP_WNDPROC, (LONG_PTR)listProc);

		// set icon
		hIconSm = (HICON)LoadImage(getInstance(), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON,
			GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
		hIcon = (HICON)LoadImage(getInstance(), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON,
			GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
		if (hIconSm)
			SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);
		if (hIcon)
			SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
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

		onInit.set_value();
		return TRUE;
	}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BUTTON1:
			if (currentMode == MODE_INPUT) {
				int input_len = GetWindowTextLengthW(edit);
				std::wstring input_str(input_len, L'\0');
				GetWindowTextW(edit, input_str.data(), input_len+1);
				SetWindowTextW(edit, L"");
				setMode(MODE_NONE);
				onInput.set_value(input_str);
			} else if (currentMode == MODE_PAUSE) {
				setMode(MODE_NONE);
				onUnpause.set_value();
			}
			return TRUE;
		case IDC_BUTTON_YES:
		case IDC_BUTTON_NO:
			if (currentMode == MODE_ASK_YN) {
				setMode(MODE_NONE);
				onYesNo.set_value(LOWORD(wParam) == IDC_BUTTON_YES ? 'y' : 'n');
			}
			return TRUE;
		case IDC_LIST1:
			switch (HIWORD(wParam)) {
			case LBN_DBLCLK: {
				int cur = ListBox_GetCurSel((HWND)lParam);
				if (currentMode == MODE_INPUT && cur != LB_ERR && (!responses[cur].empty() || cur == last_index)) {
					SetDlgItemTextW(hWnd, IDC_EDIT1, L"");
					setMode(MODE_NONE);
					onInput.set_value(responses[cur]);
				} else if (currentMode == MODE_PAUSE) {
					setMode(MODE_NONE);
					onUnpause.set_value();
				}
				return TRUE;
			}
			}
		}
		return FALSE;
	case APP_READQUEUE: {
		std::lock_guard<std::mutex> lock(mutex);
		LineEntry ent;
		while (popQueue(ent)) {
			switch (ent.type) {
			case LINE_ADD:
				last_index = ListBox_AddString(list, ent.content.c_str());
				responses.push_back(std::move(ent.response));
				last_line = std::move(ent.content);
				break;
			case LINE_APPEND: {
				last_line += ent.content;
				ListBox_DeleteString(list, last_index);
				ListBox_InsertString(list, last_index, last_line.c_str());
				break;
			}
			case LINE_CLS:
				ListBox_ResetContent(list);
				responses.clear();
				break;
			}
		}
		return TRUE;
	}
	case APP_GETINPUT:
		onInput = std::move(*(std::promise<std::wstring>*)lParam);
		ReplyMessage(0L);
		setMode(MODE_INPUT);
		return TRUE;
	case APP_PAUSE:
		onUnpause = std::move(*(std::promise<void>*)lParam);
		ReplyMessage(0L);
		setMode(MODE_PAUSE);
		return TRUE;
	case APP_ASKYN:
		onYesNo = std::move(*(std::promise<char>*)lParam);
		ReplyMessage(0L);
		setMode(MODE_ASK_YN);
		return TRUE;
	case APP_PREUPDATE:
		SetWindowRedraw(list, FALSE);
		return TRUE;
	case APP_UPDATE:
		SetWindowRedraw(list, TRUE);
		ListBox_SetCurSel(list, last_index);
		return TRUE;
	case APP_PROGRESS:
		setMarquee(wParam == -1);
		if (wParam != -1)
			ProgressBar_SetPos(progress, wParam);
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
		if (hIconSm)
			DestroyIcon(hIconSm);
		if (hIcon)
			DestroyIcon(hIcon);
		return TRUE;
	}
	return FALSE;
}
static bool needAppend = false;
static bool dontUpdate = false;
void log_windows(std::wstring_view text) {
	if (!g_console->isAlive())
		return;
	if (!dontUpdate)
		g_console->preupdate();

	while (!text.empty()) {
		size_t len = text.find(L'\n');
		std::wstring_view line = text.substr(0, len);
		
		if (needAppend == true) {
			LineEntry le = { LINE_APPEND, std::wstring(line), L"" };
			g_console->pushQueue(std::move(le));
			needAppend = false;
		} else {
			LineEntry le = { LINE_ADD, std::wstring(line), L"" };
			g_console->pushQueue(std::move(le));
		}

		if (len != -1) {
			text.remove_prefix(len + 1);
		} else { // incomplete last line
			needAppend = true;
			break;
		}
	}

	if (!dontUpdate) {
		g_console->readQueue();
		g_console->update();
	}
}

void log_nwindows(const char* text, size_t len) {
	VLA(wchar_t, text_w, len);
	size_t len_w = StringToUTF16(text_w, text, len);
	log_windows(std::wstring_view(text_w, len_w));
	VLA_FREE(text_w);
}
void log_windows(const char* text) {
	log_nwindows(text, strlen(text));
}
void con_vprintf(const char *str, va_list va)
{
	if (str)
		log_windows(to_utf16(stringf(str, va)));
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
void con_clickable(std::wstring &&response, std::wstring &&line) {
	LineEntry le = { LINE_ADD, std::move(line), std::move(response)};
	g_console->pushQueue(std::move(le));
}

static PromiseSlot<void> g_exitguithreadevent;
void console_init() {
	INITCOMMONCONTROLSEX iccex;
	iccex.dwSize = sizeof(iccex);
	iccex.dwICC = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&iccex);

	log_set_hook(log_windows, log_nwindows);

	std::promise<void> promise;
	std::future<void> future = promise.get_future();
	std::thread([&promise]() {
		g_console->onInit = std::move(promise);
		g_console->createModal(NULL);
		log_set_hook(NULL, NULL);
		if (!g_exitguithreadevent) {
			exit(0); // Exit the entire process when exiting this thread
		}
		else {
			g_exitguithreadevent.set_value();
		}
	}).detach();
	future.get();
}
std::wstring console_read() {
	dontUpdate = false;
	g_console->readQueue();
	g_console->update();
	std::wstring input = g_console->getInput().get();
	return input;
}
void cls(SHORT top) {
	LineEntry le = { LINE_CLS, L"", L"" };
	g_console->pushQueue(std::move(le));
	g_console->readQueue();
	needAppend = false;
	
	if (dontUpdate) {
		g_console->update();
		g_console->preupdate();
	}
}
void pause(void) {
	dontUpdate = false;
	con_printf("Press ENTER to continue . . .\n"); // this will ReadQueue and Update for us
	g_console->pause().get();
}
void console_prepare_prompt(void) {
	g_console->preupdate();
	dontUpdate = true;
}
void console_print_percent(int pc) {
	g_console->setProgress(pc);
}
char console_ask_yn(const char* what) {
	dontUpdate = false;
	log_windows(what);
	needAppend = false;

	return g_console->askyn().get();
}
void con_end(void) {
	std::promise<void> promise;
	std::future<void> future = promise.get_future();
	g_exitguithreadevent = std::move(promise);
	SendMessage(g_console->getHandle(), WM_CLOSE, 0, 0L);
	future.get();
}
HWND con_hwnd(void) {
	return g_console->getHandle();
}
int console_width(void) {
	return 80;
}
