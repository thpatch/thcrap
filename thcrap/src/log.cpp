/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Logging functions.
  */

#include "thcrap.h"
#include <queue>
#include <array>

// -------
// Globals
// -------

struct log_string_t {
	const char* str;
	uint32_t n;
	bool is_n;

	inline constexpr log_string_t(const char* str, uint32_t n, bool is_n) : str(str), n(n), is_n(is_n) {}
};

static HANDLE log_file = INVALID_HANDLE_VALUE;
static bool console_open = false;
static HANDLE log_thread_handle = INVALID_HANDLE_VALUE;
static std::queue<log_string_t> log_queue;
static SRWLOCK queue_srwlock = { SRWLOCK_INIT };
static volatile bool log_is_empty = true;
static bool async_enabled = false;

// Config
DWORD log_async = true;

// For checking nested thcrap instances that access the same log file.
// We only want to print an error message for the first instance.
static HANDLE log_filemapping = INVALID_HANDLE_VALUE;
static constexpr char LOG[] = "logs/thcrap_log.txt";
static constexpr char LOG_ROTATED[] = "logs/thcrap_log.%d.txt";
static constexpr int ROTATIONS = 5; // Number of backups to keep
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
	snprintf(str->str, sizeof(str->str), "%lu", err);
	return str->str;
}

const char* lasterror_str()
{
	return lasterror_str_for(GetLastError());
}

void log_set_hook(void(*print_hook)(const char*), void(*nprint_hook)(const char*,size_t)){
	log_print_hook = print_hook;
	log_nprint_hook = nprint_hook;
}

// Rotation
// --------
void log_rotate(void)
{
	char file_one[sizeof(LOG) + DECIMAL_DIGITS_BOUND(int) + 1];
	char file_two[sizeof(LOG) + DECIMAL_DIGITS_BOUND(int) + 1];

	char* rot_to = &file_one[0];
	char* rot_from = &file_two[0];

	int rotation = ROTATIONS;
	sprintf(rot_to, LOG_ROTATED, rotation);
	while (--rotation) {
		sprintf(rot_from, LOG_ROTATED, rotation);
		MoveFileExU(rot_from, rot_to, MOVEFILE_REPLACE_EXISTING);
		std::swap(rot_from, rot_to);
	}
	MoveFileExU(LOG, rot_to, MOVEFILE_REPLACE_EXISTING);
}
// --------

static void log_print_real(const char* str, uint32_t n, bool is_n) {
	static DWORD byteRet;
	if unexpected(console_open) {
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), str, n, &byteRet, NULL);
	}
	if (HANDLE file = log_file) {
		WriteFile(file, str, n, &byteRet, NULL);
	}
	if (!is_n) {
		if (auto func = log_print_hook) {
			func(str);
		}
	} else {
		if (auto func = log_nprint_hook) {
			func(str, n);
		}
	}
}

static DWORD WINAPI log_thread(LPVOID lpParameter) {
	for (;;) {
		AcquireSRWLockExclusive(&queue_srwlock);
		if (log_queue.empty()) {
			log_is_empty = true;
			ReleaseSRWLockExclusive(&queue_srwlock);
			while (log_is_empty) Sleep(10);
		} else {
			log_string_t log_str = std::move(log_queue.front());
			log_queue.pop();
			ReleaseSRWLockExclusive(&queue_srwlock);

			log_print_real(log_str.str, log_str.n, log_str.is_n);
			free((void*)log_str.str);
		}
	}
}

static void log_push(const char* str, uint32_t n, bool is_n) {
	if (async_enabled) {
		char* new_str = (char*)malloc(n + 1);
		new_str[n] = '\0';
		memcpy(new_str, str, n);
		AcquireSRWLockExclusive(&queue_srwlock);
		log_queue.emplace(new_str, n, is_n);

		log_is_empty = false;
		ReleaseSRWLockExclusive(&queue_srwlock);
	} else {
		log_print_real(str, n, is_n);
	}
}

void log_nprint(const char* str, uint32_t n) {
	log_push(str, (uint32_t)strnlen(str, n), true);
}

void log_print(const char *str) {
	log_push(str, (uint32_t)strlen(str), false);
}

void log_printw(const wchar_t *wstr) {
	size_t buffer_size = (wcslen(wstr) + 1) * UTF8_MUL;
	VLA(char, str, buffer_size);
	StringToUTF8(str, wstr, buffer_size);
	log_print(str);
}

inline void log_vprintf(const char *format, va_list va) {
	va_list va2;
	va_copy(va2, va);
	const int total_size = vsnprintf(NULL, 0, format, va2);
	va_end(va2);
	if (total_size > 0) {
		VLA(char, str, total_size + 1);
		vsprintf(str, format, va);
		log_push(str, total_size, false);
		VLA_FREE(str);
	}
}

void log_printf(const char *format, ...) {
	va_list va;
	va_start(va, format);
	log_vprintf(format, va);
	va_end(va);
}

void log_flush() {
	if (async_enabled) {
	AcquireSRWLockExclusive(&queue_srwlock);
		while (!log_queue.empty()) {
			log_string_t log_str = std::move(log_queue.front());
			log_queue.pop();
			log_print_real(log_str.str, log_str.n, log_str.is_n);
			free((void*)log_str.str);
		}
	ReleaseSRWLockExclusive(&queue_srwlock);
	}
	FlushFileBuffers(GetStdHandle(STD_OUTPUT_HANDLE));
	FlushFileBuffers(log_file);
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
	log_printf(
		"---------------------------\n"
		"%s\n"
		"---------------------------\n"
		, text
	);
	return MessageBox(guess_mbox_owner(), text, (caption ? caption : PROJECT_NAME), type);
}

int log_vmboxf(const char *caption, const UINT type, const char *format, va_list va)
{
	int ret = 0;
	if(format) {
		va_list va2;
		va_copy(va2, va);
		const int total_size = vsnprintf(NULL, 0, format, va2);
		va_end(va2);
		if (total_size > 0) {
			VLA(char, formatted_str, total_size + 1);
			vsprintf(formatted_str, format, va);
			ret = log_mbox(caption, type, formatted_str);
			VLA_FREE(formatted_str);
		}
	}
	return ret;
}

int log_mboxf(const char *caption, const UINT type, const char *format, ...)
{
	va_list va;
	va_start(va, format);
	int ret = log_vmboxf(caption, type, format, va);
	va_end(va);
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

	console_open = true;
}

#if CPP20
static auto line = []() consteval {
	constexpr char8_t dash_char[] = u8"―";
	constexpr char project_name[] = "Touhou Community Reliant Automatic Patcher";
	constexpr char logfile_str[] = " logfile";
	constexpr size_t bytes_per_dash = sizeof(dash_char) - 1;
	constexpr size_t width = sizeof(project_name) - 1 + sizeof(logfile_str) - 1;
	std::array<char8_t, bytes_per_dash * width + 1> ret = {};
	for (size_t i = 0; i < width; ++i) {
		for (size_t j = 0; j < bytes_per_dash; ++j) {
			ret[i * bytes_per_dash + j] = dash_char[j];
		}
	}
	return ret;
}();
#endif

void log_init(int console)
{
	CreateDirectoryU("logs", NULL);
	if constexpr (ROTATIONS > 0) {
		log_rotate();
	}

	// Using CreateFile, _open_osfhandle and _fdopen instead of plain fopen because we need the flag FILE_SHARE_DELETE for log rotation
	log_file = CreateFileU(LOG, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(log_file) {

#if !CPP20
		constexpr std::string_view DashUChar = u8"―";

		const size_t line_len = (strlen(PROJECT_NAME) + strlen(" logfile")) * DashUChar.length();
		VLA(char, line, line_len + 1);
		line[line_len] = '\0';
		
		for (size_t i = 0; i < line_len; i += DashUChar.length()) {
			memcpy(&line[i], DashUChar.data(), DashUChar.length());
		}
#endif

		log_printf(
			"%s\n"
			"%s logfile\n"
			"Branch: %s\n"
			"Version: %s\n"
			, &line[0]
			, PROJECT_NAME
			, PROJECT_BRANCH
			, PROJECT_VERSION_STRING
		);
		{
			static constexpr const char* months[] = {
				"Invalid",
				"Jan",
				"Feb",
				"Mar",
				"Apr",
				"May",
				"Jun",
				"Jul",
				"Aug",
				"Sep",
				"Oct",
				"Nov",
				"Dec"
			};
			SYSTEMTIME time;
			GetSystemTime(&time);
			if (time.wMonth > 12) time.wMonth = 0;
			log_printf("Current time: %s %d %d %d:%02d:%02d\n",
				months[time.wMonth], time.wDay, time.wYear,
				time.wHour, time.wMinute, time.wSecond);
		}
#if defined(BUILDER_NAME_W)
		{
			static constexpr wchar_t builder[] = BUILDER_NAME_W;
			UTF8_DEC(builder);
			UTF8_CONV(builder);
			log_printf(
				"Build time: "  __DATE__ " " __TIME__ "\n"
				"Built by: %s\n"
				, builder_utf8
			);
			UTF8_FREE(builder);
		}
#elif defined(BUILDER_NAME)
		log_printf(
			"Build time: "  __DATE__ " " __TIME__ "\n"
			"Built by: %s\n"
			, BUILDER_NAME
		);
#endif
		log_printf(
			"Command line: %s\n"
			"\nSystem Information:\n"
			, GetCommandLineU()
		);

		{
			char cpu_brand[48] = {};
			__cpuidex((int*)cpu_brand, 0x80000002, 0);
			__cpuidex((int*)cpu_brand + 4, 0x80000003, 0);
			__cpuidex((int*)cpu_brand + 8, 0x80000004, 0);
			log_printf("CPU: %s\n", cpu_brand);
		}

		{
			MEMORYSTATUSEX ram_stats = { sizeof(MEMORYSTATUSEX) };
			GlobalMemoryStatusEx(&ram_stats);

			double ram_total = (double)ram_stats.ullTotalPhys;
			int div_count_total = 0;
			while (ram_total >= 1024.0) {
				ram_total /= 1024.0;
				++div_count_total;
			}

			double ram_left = (double)ram_stats.ullAvailPhys;
			int div_count_left = 0;
			while (ram_left >= 1024.0) {
				ram_left /= 1024.0;
				++div_count_left;
			}

			static constexpr const char* size_units[] = {
				"B",
				"KiB",
				"MiB",
				"GiB",
				"TiB",
				"PiB"
			};

			log_printf("RAM: %.2f%s free out of %.1f%s, %d%% used\n",
				ram_left, size_units[div_count_left],
				ram_total, size_units[div_count_total],
				ram_stats.dwMemoryLoad
			);
		}

		log_printf(
			"OS/Runtime: %s\n"
			"Code pages: ANSI=%u, OEM=%u\n"
			"\nScreens:\n"
			, windows_version()
			, GetACP(), GetOEMCP()
		);

		{
			DISPLAY_DEVICEA display_device = { sizeof(display_device) };
			for (
				DWORD i = 0;
				EnumDisplayDevicesA(NULL, i, &display_device, EDD_GET_DEVICE_INTERFACE_NAME);
				++i
			) {
				if (display_device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
					DISPLAY_DEVICEA mon = { sizeof(mon) };
					if (!EnumDisplayDevicesA(display_device.DeviceName, 0, &mon, EDD_GET_DEVICE_INTERFACE_NAME)) {
						continue;
					}

					log_printf("%s on %s: ", mon.DeviceString, display_device.DeviceString);

					DEVMODEA d = { sizeof(d) };
					EnumDisplaySettingsA(display_device.DeviceName, ENUM_CURRENT_SETTINGS, &d);
					if ((d.dmFields & DM_PELSHEIGHT) && !(d.dmFields & DM_PAPERSIZE)) {
						log_printf("%dx%d@%d %dHz\n", d.dmPelsWidth, d.dmPelsHeight, d.dmBitsPerPel, d.dmDisplayFrequency);
					}
				}
			}
		}

		log_printf("%s\n\n", &line[0]);
		
		FlushFileBuffers(log_file);
#if !CPP20
		VLA_FREE(line);
#endif
	}
	if (console) {
		OpenConsole();
	}
	DWORD cur_dir_len = GetCurrentDirectoryU(0, nullptr);
	size_t full_fn_len = (size_t)cur_dir_len + sizeof(LOG);
	VLA(char, full_fn, full_fn_len);
	GetCurrentDirectoryU(cur_dir_len, full_fn);
	full_fn[cur_dir_len - 1] = '/';
	full_fn[cur_dir_len] = '\0';
	str_slash_normalize(full_fn); // Necessary!
	memcpy(full_fn + cur_dir_len, LOG, sizeof(LOG));

	log_filemapping = CreateFileMappingU(
		INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 1, full_fn
	);
	if(log_file == INVALID_HANDLE_VALUE && GetLastError() != ERROR_ALREADY_EXISTS) {
		auto ret = log_mboxf(nullptr, MB_OKCANCEL | MB_ICONHAND,
			"Error creating %s: %s\n"
			"\n"
			"Logging will be unavailable. "
			"Further writes to this directory are likely to fail as well. "
			"Moving %s to a different directory will probably fix this.\n"
			"\n"
			"Continue?",
			full_fn, strerror(errno), PROJECT_NAME_SHORT
		);
		if(ret == IDCANCEL) {
			auto pExitProcess = ((void (TH_STDCALL*)(UINT))detour_top(
				"kernel32.dll", "ExitProcess", (FARPROC)thcrap_ExitProcess
			));
			VLA_FREE(full_fn);
			pExitProcess(-1);
			TH_UNREACHABLE;
		}
	}
	if (log_async) {
		log_thread_handle = CreateThread(0, 0, log_thread, NULL, 0, NULL);
		async_enabled = true;
	}
	VLA_FREE(full_fn);
}

void log_exit(void) {
	log_flush();
	if (async_enabled) {
		TerminateThread(log_thread_handle, 0);
	}
	if (console_open) {
		FreeConsole();
	}
	if (log_file) {
		CloseHandle(log_filemapping);
		CloseHandle(log_file);
		log_file = INVALID_HANDLE_VALUE;
	}
}


bool BP_log(x86_reg_t* regs, json_t* bp_info) {
	const char* format = json_object_get_string(bp_info, "format");
	if TH_UNLIKELY(!format) {
		return true;
	}

	auto* args = json_object_get(bp_info, "args");
	if (!json_is_array(args)) {
		log_print(format);
		return true;
	}

	size_t num_args = json_array_size(args);

	VLA(uintptr_t, args_buf, num_args);

	for (size_t i = 0; i < num_args; i++) {
		args_buf[i] = json_immediate_value(json_array_get(args, i), regs);
	}

	log_vprintf(format, (va_list)args_buf);
	VLA_FREE(args_buf);

	return true;
}
