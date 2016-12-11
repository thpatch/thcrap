/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Abstractions and approximations of Win32 API features that are not
  * available on Windows XP.
  */

#pragma once

/// Slim Reader/Writer Locks
/// ------------------------
typedef void __stdcall srwlock_func_t(PSRWLOCK SRWLock);

#define AcquireSRWLockExclusive srwlock_funcs[0]
#define ReleaseSRWLockExclusive srwlock_funcs[1]
#define AcquireSRWLockShared srwlock_funcs[2]
#define ReleaseSRWLockShared srwlock_funcs[3]

THCRAP_API extern srwlock_func_t *srwlock_funcs[4];
/// ------------------------
