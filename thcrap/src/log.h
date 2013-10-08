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

/// ---------------
/// Standard output
/// ---------------
// Basic
void log_print(const char *text);
// Specific length
void log_nprint(const char *text, size_t n);
// Formatted
void log_vprintf(const char *text, va_list va);
void log_printf(const char *text, ...);
/// ---------------

/// -------------
/// Message boxes
// Technically not a "logging function", but hey, it has variable arguments.
/// -------------
// Basic
int log_mbox(const char *caption, const UINT type, const char *text);
// Formatted
int log_vmboxf(const char *caption, const UINT type, const char *text, va_list va);
int log_mboxf(const char *caption, const UINT type, const char *text, ...);
/// -------------

void log_init(int console);
void log_exit();
