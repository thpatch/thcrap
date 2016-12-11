/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Abstractions and approximations of Win32 API features that are not
  * available on Windows XP.
  */

#include "thcrap.h"

/// Slim Reader/Writer Locks
/// ------------------------
THCRAP_API srwlock_func_t *srwlock_funcs[4];

#define SPIN_UNTIL(cond) while(!(cond)) { Sleep(1); }

static int swrlock_init = []
{
	auto kernel32 = GetModuleHandleA("kernel32.dll");
	AcquireSRWLockExclusive = (srwlock_func_t *)GetProcAddress(kernel32, "AcquireSRWLockExclusive");
	ReleaseSRWLockExclusive = (srwlock_func_t *)GetProcAddress(kernel32, "ReleaseSRWLockExclusive");
	AcquireSRWLockShared = (srwlock_func_t *)GetProcAddress(kernel32, "AcquireSRWLockShared");
	ReleaseSRWLockShared = (srwlock_func_t *)GetProcAddress(kernel32, "ReleaseSRWLockShared");
	if(
		srwlock_funcs[0] == nullptr || srwlock_funcs[1] == nullptr ||
		srwlock_funcs[2] == nullptr || srwlock_funcs[3] == nullptr
	) {
		// XP approximations. We simply treat the SRW lock
		// value as an integer with the following values:
		// • -1: exclusive
		// •  0: released
		// • >0: shared
		AcquireSRWLockExclusive = [] (PSRWLOCK SRWLock) {
			long *val = (long *)&SRWLock->Ptr;
			SPIN_UNTIL(*val == 0);
			while(*val != -1) {
				InterlockedCompareExchange(val, -1, 0);
			}
		};
		ReleaseSRWLockExclusive = [] (PSRWLOCK SRWLock) {
			long *val = (long *)&SRWLock->Ptr;
			assert(*val == -1);
			while(*val != 0) {
				InterlockedCompareExchange(val, 0, -1);
			}
		};
		AcquireSRWLockShared = [] (PSRWLOCK SRWLock) {
			long *val = (long *)&SRWLock->Ptr;
			SPIN_UNTIL(*val >= 0);
			InterlockedIncrement(val);
		};
		ReleaseSRWLockShared = [] (PSRWLOCK SRWLock) {
			long *val = (long *)&SRWLock->Ptr;
			assert(*val >= 1);
			InterlockedDecrement(val);
		};
	}
	return 0;
}();
/// ------------------------
