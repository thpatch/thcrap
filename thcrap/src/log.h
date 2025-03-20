/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Logging functions.
  * Log to both a file and, if requested, an on-screen console.
  *
  * As of now, we do not enforce char strings to be in UTF-8.
  */

#pragma once

// Config
THCRAP_API extern DWORD log_async;

// Returns a string representation of the given Win32 error code.
// Currently in English and fairly abbreviated compared to FormatMessage().
THCRAP_API const char* lasterror_str_for(DWORD err);
// Calls GetLastError() itself.
THCRAP_API const char* lasterror_str();

THCRAP_API void log_set_hook(void(*print_hook)(const char*), void(*nprint_hook)(const char*, size_t));

/// ---------------
/// Standard output
/// ---------------
// Basic
THCRAP_API void log_print(const char *text);
THCRAP_API void log_printw(const wchar_t *text);
// Specific length
THCRAP_API void log_nprint(const char *text, size_t n);
// Formatted
THCRAP_API void log_vprintf(const char *format, va_list va);
THCRAP_API void log_printf(const char *format, ...);
#ifdef NDEBUG
// Using __noop makes the compiler check the validity of the
// macro contents for syntax errors without actually compiling them.
# define log_debugf(...) TH_EVAL_NOOP(__VA_ARGS__)
# define log_debug(...) TH_EVAL_NOOP(__VA_ARGS__)
#else
# define log_debugf(...) log_printf(__VA_ARGS__)
# define log_debug(...) log_print(__VA_ARGS__)
#endif

#ifdef _MSC_VER
# define log_func_printf(format, ...) \
	log_printf("[" __FUNCTION__ "]: " format, ##__VA_ARGS__)
# define log_func_print(...) \
	log_print("[" __FUNCTION__ "]: " __VA_ARGS__)
#else
# define log_func_printf(format, ...) \
	log_printf("[%s]: " format, __func__, ##__VA_ARGS__)
# define log_func_print(...) \
	log_printf("[%s]: " __VA_ARGS__ , __func__)
#endif

// Flush the log file
THCRAP_API void log_flush();
/// ---------------

/// -------------
/// Message boxes
// Technically not a "logging function", but hey, it has variable arguments.
/// -------------
// Basic
THCRAP_API int log_mbox(const char *caption, const UINT type, const char *text);
// Formatted
THCRAP_API int log_vmboxf(const char *caption, const UINT type, const char *format, va_list va);
THCRAP_API int log_mboxf(const char *caption, const UINT type, const char *format, ...);
// Set the owner hwnd for the log_mbox* functions
THCRAP_API void log_mbox_set_owner(HWND hwnd);

#define log_error_mbox(caption, text) log_mbox(caption, MB_OK | MB_ICONERROR, text)
#define log_error_mboxf(caption, ...) log_mboxf(caption, MB_OK | MB_ICONERROR, __VA_ARGS__)
/// -------------

THCRAP_INTERNAL_API void log_init(int console);
THCRAP_INTERNAL_API void log_exit(void);
