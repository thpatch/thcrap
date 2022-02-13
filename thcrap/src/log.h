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
const char* lasterror_str_for(DWORD err);
// Calls GetLastError() itself.
const char* lasterror_str();

void log_set_hook(void(*print_hook)(const char*), void(*nprint_hook)(const char*, size_t));

/// ---------------
/// Standard output
/// ---------------
// Basic
void log_print(const char *text);
// Specific length
void log_nprint(const char *text, size_t n);
// Formatted
void log_vprintf(const char *format, va_list va);
void log_printf(const char *format, ...);
#ifdef NDEBUG
// Using __noop makes the compiler check the validity of the
// macro contents for syntax errors without actually compiling them.
# define log_debugf(...) TH_EVAL_NOOP(__VA_ARGS__)
#else
# define log_debugf(...) log_printf(__VA_ARGS__)
#endif

#ifdef _MSC_VER
# define log_func_printf(format, ...) \
	log_printf("[" __FUNCTION__ "]: " format, ##__VA_ARGS__)
#else
# define log_func_printf(format, ...) \
	log_printf("[%s]: " format, __func__, ##__VA_ARGS__)
#endif

// Flush the log file
void log_flush();
/// ---------------

/// -------------
/// Message boxes
// Technically not a "logging function", but hey, it has variable arguments.
/// -------------
// Basic
int log_mbox(const char *caption, const UINT type, const char *text);
// Formatted
int log_vmboxf(const char *caption, const UINT type, const char *format, va_list va);
int log_mboxf(const char *caption, const UINT type, const char *format, ...);
// Set the owner hwnd for the log_mbox* functions
void log_mbox_set_owner(HWND hwnd);
/// -------------

/// Per-module loggers
/// ------------------
#ifdef __cplusplus
extern "C++" {

class logger_t {
	const char *err_caption;
	std::string_view prefix;

	constexpr logger_t(const char* err_caption, std::string_view prefix)
		: err_caption(err_caption), prefix(prefix) {
	}

public:
	THCRAP_API virtual std::nullptr_t verrorf(const char *text, va_list va) const;
	THCRAP_API virtual std::nullptr_t errorf(const char *text, ...) const;

	// Returns a new logger that prepends [prefix] to all messages.
	constexpr logger_t prefixed(const char *prefix) const {
		return logger_t(err_caption, prefix);
	}

	constexpr logger_t(const char *err_caption)
		: err_caption(err_caption), prefix({"", 0}) {
	}
};

}
#endif
/// ------------------

void log_init(int console);
void log_exit(void);
