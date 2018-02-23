#include <thcrap.h>
#include <windows.h>
#include "console.h"
void console_init()
{
	int wine_flag = GetProcAddress(
		GetModuleHandleA("kernel32.dll"), "wine_get_unix_file_name"
	) != 0;

	// Necessary to correctly process *any* input of non-ASCII characters
	// in the console subsystem
	w32u8_set_fallback_codepage(GetOEMCP());

	// Maximize the height of the console window... unless we're running under
	// Wine, where this 1) doesn't work and 2) messes up the console buffer
	if (!wine_flag) {
		CONSOLE_SCREEN_BUFFER_INFO sbi = { 0 };
		HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
		COORD largest = GetLargestConsoleWindowSize(console);
		HWND console_wnd = GetConsoleWindow();
		RECT console_rect;

		GetWindowRect(console_wnd, &console_rect);
		SetWindowPos(console_wnd, NULL, console_rect.left, 0, 0, 0, SWP_NOSIZE);
		GetConsoleScreenBufferInfo(console, &sbi);
		sbi.srWindow.Bottom = largest.Y - 4;
		SetConsoleWindowInfo(console, TRUE, &sbi.srWindow);
	}
}
void con_printf(const char *str, ...) {
	if (str) {
		va_list va;
		va_start(va, str);
		vprintf(str, va);
		VLA_FREE(str_full);
		va_end(va);
	}
}
void con_clickable(const char *response) {}
void con_clickable(int response) {}
char* console_read(char *str, int n)
{
	int ret;
	int i;
	fgets(str, n, stdin);
	{
		// Ensure UTF-8
		VLA(wchar_t, str_w, n);
		StringToUTF16(str_w, str, n);
		StringToUTF8(str, str_w, n);
		VLA_FREE(str_w);
	}
	// Get rid of the \n
	for (i = 0; i < n; i++) {
		if (str[i] == '\n') {
			str[i] = 0;
			return str;
		}
	}
	while ((ret = getchar()) != '\n' && ret != EOF);
	return str;
}

void cls(SHORT top)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// here's where we'll home the cursor
	COORD coordScreen = { 0, top };

	DWORD cCharsWritten;
	// to get buffer info
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	// number of character cells in the current buffer
	DWORD dwConSize;

	// get the number of character cells in the current buffer
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

	// fill the entire screen with blanks
	FillConsoleOutputCharacter(
		hConsole, TEXT(' '), dwConSize, coordScreen, &cCharsWritten
	);
	// get the current text attribute
	GetConsoleScreenBufferInfo(hConsole, &csbi);

	// now set the buffer's attributes accordingly
	FillConsoleOutputAttribute(
		hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten
	);

	// put the cursor at (0, 0)
	SetConsoleCursorPosition(hConsole, coordScreen);
	return;
}

// http://support.microsoft.com/kb/99261
// Because I've now learned how bad system("pause") can be
void pause(void)
{
	int ret;
	printf("Press ENTER to continue . . . ");
	while ((ret = getchar()) != '\n' && ret != EOF);
}
void console_prepare_prompt(void) {
	/* do nothing */
}
void console_print_percent(int pc) {
	con_printf("%3d%%\b\b\b\b", pc);
}
