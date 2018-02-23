#include <thcrap.h>
#include <windows.h>
#include <windowsx.h>
#include "console.h"
#include "resource.h"
#include <string.h>
#include <CommCtrl.h>
#include <queue>
#include <string>

// TODO: preferrably do a separate manifest file
#pragma comment(lib,"Comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define APP_READQUEUE (WM_APP+0)
/* wparam = length of output string */
#define APP_GETINPUT (WM_APP+2)
#define APP_UPDATE (WM_APP+4)
#define APP_PREUPDATE (WM_APP+5)
#define APP_PAUSE (WM_APP+6)
/* wparam = percent */
#define APP_PROGRESS (WM_APP+7)

/* Reads line queue */
#define Thcrap_ReadQueue(hwndDlg) ((void)::PostMessage((hwndDlg), APP_READQUEUE, 0, 0L))
/* Request input */
/* UI thread will signal g_event when user confirms input */
#define Thcrap_GetInput(hwndDlg,len) ((void)::PostMessage((hwndDlg), APP_GETINPUT, (WPARAM)(DWORD)(len), 0L))
/* Should be called after adding/appending bunch of lines */
#define Thcrap_Update(hwndDlg) ((void)::PostMessage((hwndDlg), APP_UPDATE, 0, 0L))
/* Should be called before adding lines*/
#define Thcrap_Preupdate(hwndDlg) ((void)::PostMessage((hwndDlg), APP_PREUPDATE, 0, 0L))
/* Pause */
#define Thcrap_Pause(hwndDlg) ((void)::PostMessage((hwndDlg), APP_PAUSE, 0, 0L))
/* Updates progress bar */
#define Thcrap_Progres(hwndDlg, pc) ((void)::PostMessage((hwndDlg), APP_PROGRESS, (WPARAM)(DWORD)(pc), 0L))

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
struct mutex_lock {
	mutex_lock(HANDLE mutex) {
		m_mutex = mutex;
		WaitForSingleObject(mutex, INFINITE);
	}
	mutex_lock& operator=(const mutex_lock&) = delete;
	mutex_lock(const mutex_lock&) = delete;
	~mutex_lock() {
		if(m_mutex) ReleaseMutex(m_mutex);
		m_mutex = nullptr;
	}

private:
	HANDLE m_mutex;
};

static HWND g_hwnd = NULL;
static HANDLE g_event = NULL; // used for communicating when the input is done
static wchar_t *g_input = NULL;
static HANDLE g_mutex = NULL; // used for synchronizing the queue
static std::queue<LineEntry> g_queue;
static std::vector<std::wstring> q_responses;
static void InputMode(HWND hwndDlg) {
	ShowWindow(GetDlgItem(hwndDlg, IDC_BUTTON1), SW_SHOW);
	ShowWindow(GetDlgItem(hwndDlg, IDC_EDIT1), SW_SHOW);
	ShowWindow(GetDlgItem(hwndDlg, IDC_PROGRESS1), SW_HIDE);
}
static void ProgressBarMode(HWND hwndDlg) {
	ShowWindow(GetDlgItem(hwndDlg, IDC_BUTTON1), SW_HIDE);
	ShowWindow(GetDlgItem(hwndDlg, IDC_EDIT1), SW_HIDE);
	ShowWindow(GetDlgItem(hwndDlg, IDC_PROGRESS1), SW_SHOW);
}

INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static int input_len = 0;
	static int last_index = 0;
	static bool input = false, pause = false;
	static std::wstring pending = L"";
	switch (uMsg) {
	case WM_INITDIALOG: {
		g_hwnd = hwndDlg;

		SendMessage(GetDlgItem(hwndDlg, IDC_PROGRESS1), PBM_SETRANGE, 0, MAKELONG(0, 100));

		// set icon
		HICON hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON,
			GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
		if (hIcon) {
			SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		}

		SetEvent(g_event);
		return TRUE;
	}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BUTTON1:
			if (!pause) {
				g_input = new wchar_t[input_len];
				GetDlgItemTextW(hwndDlg, IDC_EDIT1, g_input, input_len);
				SetDlgItemTextW(hwndDlg, IDC_EDIT1, L"");
				Edit_Enable(GetDlgItem(hwndDlg, IDC_EDIT1), FALSE);
			}
			input = pause = false;

			Button_Enable(GetDlgItem(hwndDlg, IDC_BUTTON1), FALSE);
			SetEvent(g_event);
			return TRUE;
		case IDC_LIST1:
			switch (HIWORD(wParam)) {
			case LBN_DBLCLK:
				int cur = ListBox_GetCurSel((HWND) lParam);
				if (input && cur != LB_ERR && !q_responses[cur].empty() ) {
					g_input = new wchar_t[q_responses[cur].length() + 1];
					wcscpy(g_input, q_responses[cur].c_str());
					SetDlgItemTextW(hwndDlg, IDC_EDIT1, L"");
					Edit_Enable(GetDlgItem(hwndDlg, IDC_EDIT1), FALSE);
					Button_Enable(GetDlgItem(hwndDlg, IDC_BUTTON1), FALSE);
					input = false;
					SetEvent(g_event);
				}
				return TRUE;
			}
		}
		return FALSE;
	case APP_READQUEUE: {
		HWND list = GetDlgItem(hwndDlg, IDC_LIST1);
		mutex_lock lock(g_mutex);
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
	}
	case APP_GETINPUT:
		InputMode(hwndDlg);
		input_len = wParam;
		input = true;
		Edit_Enable(GetDlgItem(hwndDlg, IDC_EDIT1), TRUE);
		Button_Enable(GetDlgItem(hwndDlg, IDC_BUTTON1), TRUE);
		return TRUE;
	case APP_PREUPDATE: {
		HWND list = GetDlgItem(hwndDlg, IDC_LIST1);
		SetWindowRedraw(list, FALSE);
		return TRUE;
	}
	case APP_UPDATE: {
		HWND list = GetDlgItem(hwndDlg, IDC_LIST1);
		SendMessage(list, WM_VSCROLL, SB_BOTTOM, 0L);
		ListBox_SetCurSel(list, last_index);// Just scrolling doesn't work well
		SetWindowRedraw(list, TRUE);
		// this causes unnescessary flickering
		//RedrawWindow(list, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
		return TRUE;
	}
	case APP_PAUSE:
		InputMode(hwndDlg);
		pause = true;
		Button_Enable(GetDlgItem(hwndDlg, IDC_BUTTON1), TRUE);
		return TRUE;
	case APP_PROGRESS:
		ProgressBarMode(hwndDlg);
		SendMessage(GetDlgItem(hwndDlg, IDC_PROGRESS1), PBM_SETPOS, wParam, 0L);
		return TRUE;
	case WM_SIZE: {
		RECT baseunits;
		baseunits.left = baseunits.right = 100;
		baseunits.top = baseunits.bottom = 100;
		MapDialogRect(hwndDlg, &baseunits);
		RECT r, r2;
		int basex = 4 * baseunits.right / 100;
		int basey = 8 * baseunits.bottom / 100;
		long origright = LOWORD(lParam) * 4 / basex;
		r.right = origright;
		r.bottom = HIWORD(lParam) * 8 / basey;
		r.right -= 7; // right margin 
		r.bottom -= 7; // bottom margin
					   // Button size
		r.left = r.right - 50;
		r.top = r.bottom - 14;
		r2 = r;
		MapDialogRect(hwndDlg, &r2);
		MoveWindow(GetDlgItem(hwndDlg, IDC_BUTTON1), r2.left, r2.top, r2.right - r2.left, r2.bottom - r2.top, TRUE);
		// Progress size
		DWORD button_left = r.left;
		r.left = 7; // left margin
		r2 = r;
		MapDialogRect(hwndDlg, &r2);
		MoveWindow(GetDlgItem(hwndDlg, IDC_PROGRESS1), r2.left, r2.top, r2.right - r2.left, r2.bottom - r2.top, TRUE);
		// Edit size
		r.right = button_left - 2;
		r2 = r;
		MapDialogRect(hwndDlg, &r2);
		MoveWindow(GetDlgItem(hwndDlg, IDC_EDIT1), r2.left, r2.top, r2.right - r2.left, r2.bottom - r2.top, TRUE);
		// List size
		r.bottom = r.top - 2;
		r.top = 7; // top margin
		r.right = origright - 7; // right margin
								 // left already set
		r2 = r;
		MapDialogRect(hwndDlg, &r2);
		MoveWindow(GetDlgItem(hwndDlg, IDC_LIST1), r2.left, r2.top, r2.right - r2.left, r2.bottom - r2.top, TRUE);
		return TRUE;
	}
	case WM_CLOSE:
		EndDialog(hwndDlg, 0);
		return TRUE;
	}
	return FALSE;
}
DWORD WINAPI ThreadProc(LPVOID lpParameter) {
	DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);
	exit(0); // Exit the entire process when exiting this thread
	return 0;
}
static bool needAppend = false;
static bool dontUpdate = false;
void log_windows(const char* text) {
	if (!dontUpdate) Thcrap_Preupdate(g_hwnd);

	int len = strlen(text) + 1;
	VLA(wchar_t, wtext, len);
	StringToUTF16(wtext, text, len);
	wchar_t *start = wtext, *end = wtext;
	{
		mutex_lock lock(g_mutex);
		while (end) {
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

			start = end;
		}
	}
	VLA_FREE(wtext);
	if (!end) needAppend = true;

	if (!dontUpdate) {
		Thcrap_ReadQueue(g_hwnd);
		Thcrap_Update(g_hwnd);
	}
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
		printf("%s", str_full);
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
		mutex_lock lock(g_mutex);
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

	log_set_hook(log_windows);
	g_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	HANDLE hThread = CreateThread(NULL, 0, ThreadProc, NULL, 0, NULL);
	CloseHandle(hThread);
	WaitForSingleObject(g_event, INFINITE);
}
char* console_read(char *str, int n) {
	dontUpdate = false;
	Thcrap_ReadQueue(g_hwnd);
	Thcrap_Update(g_hwnd);
	Thcrap_GetInput(g_hwnd, n);
	WaitForSingleObject(g_event, INFINITE);
	StringToUTF8(str, g_input, n);
	delete[] g_input;
	g_input = NULL;
	return str;
}
void cls(SHORT top) {
	mutex_lock lock(g_mutex);
	LineEntry le = { LINE_CLS, L"" };
	g_queue.push(le);
	Thcrap_ReadQueue(g_hwnd);
	needAppend = false;
}
void pause(void) {
	dontUpdate = false;
	con_printf("Press ENTER to continue . . . \n"); // this will ReadQueue and Update for us
	Thcrap_Pause(g_hwnd);
	WaitForSingleObject(g_event, INFINITE);
}
void console_prepare_prompt(void) {
	Thcrap_Preupdate(g_hwnd);
	dontUpdate = true;
}
void console_print_percent(int pc) {
	Thcrap_Progres(g_hwnd, pc);
}
