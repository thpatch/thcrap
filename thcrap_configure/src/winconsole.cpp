#include <thcrap.h>
#include <windows.h>
#include <windowsx.h>
#include "console.h"
#include "resource.h"
#include <string.h>
#include <CommCtrl.h>

// TODO: preferrably do a separate manifest file
#pragma comment(lib,"Comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

/* lparam = pointer to wide string to add to log (no newlines)*/
#define APP_ADDLOG (WM_APP+0)
#define APP_CLS (WM_APP+1)
/* wparam = length of output string */
#define APP_GETINPUT (WM_APP+2)
/* lparam = pointer to wide string to append */
#define APP_APPENDLOG (WM_APP+3)
#define APP_UPDATE (WM_APP+4)
#define APP_PREUPDATE (WM_APP+5)
#define APP_PAUSE (WM_APP+6)
/* wparam = percent */
#define APP_PROGRESS (WM_APP+7)

/* Adds a line to log*/
#define Thcrap_AddLog(hwndDlg,str) ((void)::SendMessage((hwndDlg), APP_ADDLOG, 0, (LPARAM)(LPCWSTR)(str)))
/* Clears log*/
#define Thcrap_Cls(hwndDlg) ((void)::SendMessage((hwndDlg), APP_CLS, 0, 0L))
/* Request input */
/* UI thread will signal g_event when user confirms input */
#define Thcrap_GetInput(hwndDlg,len) ((void)::SendMessage((hwndDlg), APP_GETINPUT, (WPARAM)(DWORD)(len), 0L))
/* Same as ADDLOG, but appends to the last line */
#define Thcrap_AppendLog(hwndDlg,str) ((void)::SendMessage((hwndDlg), APP_APPENDLOG, 0, (LPARAM)(LPCWSTR)(str)))
/* Should be called after adding/appending bunch of lines */
#define Thcrap_Update(hwndDlg) ((void)::SendMessage((hwndDlg), APP_UPDATE, 0, 0L))
/* Should be called before adding lines*/
#define Thcrap_Preupdate(hwndDlg) ((void)::SendMessage((hwndDlg), APP_PREUPDATE, 0, 0L))
/* Pause */
#define Thcrap_Pause(hwndDlg) ((void)::SendMessage((hwndDlg), APP_PAUSE, 0, 0L))
/* Updates progress bar */
#define Thcrap_Progres(hwndDlg, pc) ((void)::SendMessage((hwndDlg), APP_PROGRESS, (WPARAM)(DWORD)(pc), 0L))

static HWND g_hwnd = NULL;
static HANDLE g_event = NULL; // used for communicating when the input is done
static wchar_t *g_input = NULL;

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
    static bool pause = false;
	switch(uMsg) {
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
		switch(LOWORD(wParam)) {
		case IDC_BUTTON1:
            if (!pause) {
                g_input = (wchar_t*)malloc(input_len * sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_EDIT1, g_input, input_len);
                SetDlgItemTextW(hwndDlg, IDC_EDIT1, L"");
                Edit_Enable(GetDlgItem(hwndDlg, IDC_EDIT1), FALSE);
            }
            pause = false;
            Button_Enable(GetDlgItem(hwndDlg, IDC_BUTTON1), FALSE);
            SetEvent(g_event);
			return TRUE;
		}
		return FALSE;
    case APP_ADDLOG:
        last_index = ListBox_AddString(GetDlgItem(hwndDlg, IDC_LIST1), lParam);
        return TRUE;
    case APP_APPENDLOG: {
        HWND list = GetDlgItem(hwndDlg, IDC_LIST1);
        int origlen = ListBox_GetTextLen(list, last_index);
        int len =  origlen + wcslen((wchar_t*)lParam) + 1;
        VLA(wchar_t, wstr, len);
        ListBox_GetText(list, last_index, wstr);
        wcscat_s(wstr, len, (wchar_t*)lParam);
        ListBox_DeleteString(list, last_index);
        last_index = ListBox_AddString(list, wstr);
        VLA_FREE(wstr);
        return TRUE;
    }
	case APP_CLS:
        ListBox_ResetContent(GetDlgItem(hwndDlg, IDC_LIST1));
		return TRUE;
	case APP_GETINPUT:
        InputMode(hwndDlg);
		input_len = wParam;
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
		EndDialog(hwndDlg,0);
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
    if(!dontUpdate) Thcrap_Preupdate(g_hwnd);

	int len = strlen(text)+1;
	VLA(wchar_t,wtext,len);
	StringToUTF16(wtext,text,len);
	wchar_t *start=wtext,*end=wtext;
	while(end) {
		wchar_t c = *end++;
		if(c == '\n') {
			end[-1] = '\0'; // Replace newline with null
		}
		else if(c == '\0') {
            if (end != wtext && end == start) {
                break; // '\0' right after '\n'
            }
			end = NULL;
		}
		else continue;
        if (needAppend == true) {
            Thcrap_AppendLog(g_hwnd,start);
            needAppend = false;
        }
        else {
            Thcrap_AddLog(g_hwnd, start);
        }
		
		start = end;
	}
	VLA_FREE(wtext);
    if (!end) needAppend = true;

    if (!dontUpdate) Thcrap_Update(g_hwnd);
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
        printf("%s",str_full);
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
    Thcrap_Update(g_hwnd);
    dontUpdate = false;
    Thcrap_GetInput(g_hwnd, n);
	WaitForSingleObject(g_event,INFINITE);
	StringToUTF8(str, g_input, n);
	free(g_input);
	g_input = NULL;
	return str;
}
void cls(SHORT top) {
    Thcrap_Cls(g_hwnd);
    needAppend = false;
}
void pause(void) {
    con_printf("Press ENTER to continue . . . \n");
    Thcrap_Update(g_hwnd);
    dontUpdate = false;
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