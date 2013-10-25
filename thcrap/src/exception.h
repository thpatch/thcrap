/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Custom exception handler.
  */

#pragma once

// Our custom exception filter function.
LONG WINAPI exception_filter(LPEXCEPTION_POINTERS lpEI);

LPTOP_LEVEL_EXCEPTION_FILTER WINAPI exception_SetUnhandledExceptionFilter(
	__in_opt LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter
);

void exception_init(void);
void exception_detour(HMODULE hMod);
void exception_exit(void);
