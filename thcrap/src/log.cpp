/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Logging functions.
  */

#include "thcrap.h"
#include <io.h>
#include <fcntl.h>

// -------
// Globals
// -------
static FILE *log_file = NULL;
static int console_open = 0;
// For checking nested thcrap instances that access the same log file.
// We only want to print an error message for the first instance.
static HANDLE log_filemapping = INVALID_HANDLE_VALUE;
static const char LOG[] = "logs/thcrap_log.txt";
static const char LOG_ROTATED[] = "logs/thcrap_log.%d.txt";
static const int ROTATIONS = 5; // Number of backups to keep
static void (*log_print_hook)(const char*) = NULL;
static void(*log_nprint_hook)(const char*, size_t) = NULL;
static HWND mbox_owner_hwnd = NULL; // Set by log_mbox_set_owner
// -----------------------

struct lasterror_t {
	char str[DECIMAL_DIGITS_BOUND(DWORD) + 1];
};

THREAD_LOCAL(lasterror_t, lasterror_tls, nullptr, nullptr);

const char* lasterror_str_for(DWORD err)
{
	switch(err) {
	case ERROR_SHARING_VIOLATION:
		return "File in use";
	case ERROR_MOD_NOT_FOUND:
		return "File not found";
	default: // -Wswitch...
		break;
	}
	auto str = lasterror_tls_get();
	if(!str) {
		static lasterror_t lasterror_static;
		str = &lasterror_static;
	}
	snprintf(str->str, sizeof(str->str), "%u", err);
	return str->str;
}

const char* lasterror_str()
{
	return lasterror_str_for(GetLastError());
}

void log_set_hook(void(*hookproc)(const char*), void(*hookproc2)(const char*,size_t)){
	log_print_hook = hookproc;
	log_nprint_hook = hookproc2;
}

// Rotation
// --------
void log_fn_for_rotation(char *fn, int rotnum)
{
	if(rotnum == 0) {
		strcpy(fn, LOG);
	} else {
		sprintf(fn, LOG_ROTATED, rotnum);
	}
}

void log_rotate(void)
{
	size_t rot_fn_len = max(sizeof(LOG_ROTATED), sizeof(LOG));
	VLA(char, rot_from, rot_fn_len);
	VLA(char, rot_to, rot_fn_len);

	for(int rotation = ROTATIONS; rotation > 0; rotation--) {
		log_fn_for_rotation(rot_from, rotation - 1);
		log_fn_for_rotation(rot_to, rotation);
		MoveFileExU(rot_from, rot_to, MOVEFILE_REPLACE_EXISTING);
	}

	VLA_FREE(rot_from);
	VLA_FREE(rot_to);
}
// --------

#define VLA_VSPRINTF(str, va) \
	size_t str##_full_len = _vscprintf(str, va) + 1; \
	VLA(char, str##_full, str##_full_len); \
	/* vs*n*printf is not available in msvcrt.dll. Doesn't matter anyway. */ \
	vsprintf(str##_full, str, va);

void log_print(const char *str)
{
	if(console_open) {
		fprintf(stdout, "%s", str);
	}
	if(log_file) {
		fprintf(log_file, "%s", str);
		fflush(log_file);
	}
	if(log_print_hook) {
		log_print_hook(str);
	}
}

void log_nprint(const char *str, size_t n)
{
	if(console_open) {
		fwrite(str, n, 1, stdout);
	}
	if(log_file) {
		fwrite(str, n, 1, log_file);
	}
	if (log_nprint_hook) {
		log_nprint_hook(str, n);
	}
}

void log_vprintf(const char *str, va_list va)
{
	if(str) {
		VLA_VSPRINTF(str, va);
		log_print(str_full);
		VLA_FREE(str_full);
	}
}

void log_printf(const char *str, ...)
{
	if(str) {
		va_list va;
		va_start(va, str);
		log_vprintf(str, va);
		va_end(va);
	}
}

/**
  * Message box functions.
  */

struct EnumStatus
{
	HWND hwnd;
	int w;
	int h;
};

static BOOL CALLBACK enumWindowProc(HWND hwnd, LPARAM lParam)
{
	EnumStatus *status = (EnumStatus*)lParam;

	if (!IsWindowVisible(hwnd)) {
		return TRUE;
	}

	DWORD pid;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid != GetCurrentProcessId()) {
		return TRUE;
	}

	RECT rect;
	GetWindowRect(hwnd, &rect);
	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;
	if (w * h > status->w * status->h) {
		status->hwnd = hwnd;
	}

	return TRUE;
}

static HWND guess_mbox_owner()
{
	// If an owner have been set, easy - just return it.
	if (mbox_owner_hwnd) {
		return mbox_owner_hwnd;
	}

	// Time to guess. If the current thread has an active window, it's probably a good window to steal.
	HWND hwnd = GetActiveWindow();
	if (hwnd) {
		return hwnd;
	}

	// It's getting harder. Look at all the top-level visible windows of our processes, and take the biggest one.
	EnumStatus status;
	status.hwnd = nullptr;
	status.w = 10; // Ignore windows smaller than 10x10
	status.h = 10;
	EnumWindows(enumWindowProc, (LPARAM)&status);
	if (status.hwnd) {
		return status.hwnd;
	}

	// Let's hope our process is allowed to take the focus.
	return nullptr;
}

int log_mbox(const char *caption, const UINT type, const char *text)
{
	if(!caption) {
		caption = PROJECT_NAME();
	}
	log_print("---------------------------\n");
	log_printf("%s\n", text);
	log_print("---------------------------\n");
	return MessageBox(guess_mbox_owner(), text, caption, type);
}

int log_vmboxf(const char *caption, const UINT type, const char *text, va_list va)
{
	int ret = 0;
	if(text) {
		VLA_VSPRINTF(text, va);
		ret = log_mbox(caption, type, text_full);
		VLA_FREE(text_full);
	}
	return ret;
}

int log_mboxf(const char *caption, const UINT type, const char *text, ...)
{
	int ret = 0;
	if(text) {
		va_list va;
		va_start(va, text);
		ret = log_vmboxf(caption, type, text, va);
		va_end(va);
	}
	return ret;
}

void log_mbox_set_owner(HWND hwnd)
{
	mbox_owner_hwnd = hwnd;
}

static void OpenConsole(void)
{
	if(console_open) {
		return;
	}
	AllocConsole();

	// To match the behavior of the native Windows console, Wine additionally
	// needs read rights because its WriteConsole() implementation calls
	// GetConsoleMode(), and setvbuf() because… I don't know?
	freopen("CONOUT$", "w+b", stdout);
	setvbuf(stdout, NULL, _IONBF, 0);

	/// This breaks all normal, unlogged printf() calls to stdout!
	// _setmode(_fileno(stdout), _O_U16TEXT);

	console_open = 1;
}

/// Per-module loggers
/// ------------------
std::nullptr_t logger_t::verrorf(const char *text, va_list va) const
{
	auto mbox = [this, va] (const char *str) {
		log_vmboxf(err_caption, MB_OK | MB_ICONERROR, str, va);
	};
	if(prefix) {
		std::string prefixed = prefix + std::string(text);
		mbox(prefixed.c_str());
	} else {
		mbox(text);
	}
	return nullptr;
}

std::nullptr_t logger_t::errorf(const char *text, ...) const
{
	va_list va;
	va_start(va, text);
	auto ret = verrorf(text, va);
	va_end(va);
	return ret;
}
/// ------------------

void log_init(int console)
{
	CreateDirectoryU("logs", NULL);
	log_rotate();

	// Using CreateFile, _open_osfhandle and _fdopen instead of plain fopen because we need the flag FILE_SHARE_DELETE for log rotation
	HANDLE log_handle = CreateFileU(LOG, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (log_handle != INVALID_HANDLE_VALUE) {
		int fd = _open_osfhandle((intptr_t)log_handle, _O_TEXT);
		log_file = _fdopen(fd, "wt");
	}
#ifdef _DEBUG
	OpenConsole();
#else
	if(console) {
		OpenConsole();
	}
	if(log_file) {
		size_t i;
		size_t line_len = strlen(PROJECT_NAME()) + strlen(" logfile") + 1;
		VLA(unsigned char, line, line_len * UTF8_MUL);

		for(i = 0; i < line_len - 1; i++) {
			// HARRY UP, anyone?
			line[i*3+0] = 0xe2;
			line[i*3+1] = 0x80;
			line[i*3+2] = 0x95;
		}
		line[i*3] = 0;

		fprintf(log_file, "%s\n", line);
		fprintf(log_file, "%s logfile\n", PROJECT_NAME());
		fprintf(log_file, "Branch: %s\n", PROJECT_BRANCH());
		fprintf(log_file, "Version: %s\n", PROJECT_VERSION_STRING());
		fprintf(log_file, "Build time: "  __DATE__ " " __TIME__ "\n");
#if defined(BUILDER_NAME_W)
		{
			const wchar_t *builder = BUILDER_NAME_W;
			UTF8_DEC(builder);
			UTF8_CONV(builder);
			fprintf(log_file, "Built by: %s\n", builder_utf8);
			UTF8_FREE(builder);
		}
#elif defined(BUILDER_NAME)
		fprintf(log_file, "Built by: %s\n", BUILDER_NAME);
#endif
		fprintf(log_file, "Command line: %s\n", GetCommandLineU());
		fprintf(log_file, "%s\n\n", line);
		fflush(log_file);
		VLA_FREE(line);
	}
#endif
	size_t cur_dir_len = GetCurrentDirectoryU(0, nullptr);
	size_t full_fn_len = cur_dir_len + sizeof(LOG);
	VLA(char, full_fn, full_fn_len);
	defer(VLA_FREE(full_fn));
	GetCurrentDirectoryU(cur_dir_len, full_fn);
	full_fn[cur_dir_len - 1] = '/';
	full_fn[cur_dir_len] = '\0';
	str_slash_normalize(full_fn); // Necessary!
	memcpy(full_fn + cur_dir_len, LOG, sizeof(LOG));

	log_filemapping = CreateFileMappingU(
		INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 1, full_fn
	);
	if(!log_file && GetLastError() != ERROR_ALREADY_EXISTS) {
		auto ret = log_mboxf(nullptr, MB_OKCANCEL | MB_ICONHAND,
			"Error creating %s: %s\n"
			"\n"
			"Logging will be unavailable. "
			"Further writes to this directory are likely to fail as well. "
			"Moving %s to a different directory will probably fix this.\n"
			"\n"
			"Continue?",
			full_fn, strerror(errno), PROJECT_NAME_SHORT()
		);
		if(ret == IDCANCEL) {
			auto pExitProcess = ((void (__stdcall*)(UINT))detour_top(
				"kernel32.dll", "ExitProcess", (FARPROC)thcrap_ExitProcess
			));
			pExitProcess(-1);
		}
	}
}

void log_exit(void)
{
	if(console_open) {
		FreeConsole();
	}
	if(log_file) {
		CloseHandle(log_filemapping);
		fclose(log_file);
		log_file = NULL;
	}
}
