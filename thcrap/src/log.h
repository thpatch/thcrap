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

// Basic
void log_print(const char *text);
// Specific length
void log_nprint(const char *text, size_t n);
// Formatted
void log_printf(const char *text, ...);

// It's technically not a "logging function", but with the variable arguments
// and the encoding conversion, it comes pretty close.
int log_mbox(const char *caption, const UINT type, const char *text);
int log_mboxf(const char *caption, const UINT type, const char *text, ...);

void log_init(int console);
void log_exit();
