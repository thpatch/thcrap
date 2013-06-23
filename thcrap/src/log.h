/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Logging functions.
  * - char/wchar_t input
  * - char output (converted to UTF-8 from wchar_t)
  * - Logs to both file and, if requested, on-screen console
  *
  * As of now, we do not enforce char strings to be in UTF-8.
  */

#pragma once

// Basic
void log_print(const char *text);
void log_wprint(const wchar_t *text);

// Specific length
void log_nprint(const char *text, size_t n);

// Formatted
void log_printf(const char *text, ...);
void log_wprintf(const wchar_t *text, ...);

// It's technically not a "logging function", but with the variable arguments
// and the encoding conversion, it comes pretty close.
int log_mbox(const char *caption, const UINT type, const char *text);
int log_wmbox(const wchar_t *caption, const UINT type, const wchar_t *text);
int log_mboxf(const char *caption, const UINT type, const char *text, ...);
int log_wmboxf(const wchar_t *caption, const UINT type, const wchar_t *text, ...);

void log_init(int console);
void log_exit();
