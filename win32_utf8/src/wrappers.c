/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * Generic call wrappers to cut down redundancy.
  */

#include "win32_utf8.h"
#include "wrappers.h"

DWORD WINAPI Wrap1P(
	__in Wrap1PFunc_t func,
	__in LPCSTR lpsz
)
{
	BOOL ret;
	WCHAR_T_DEC(lpsz);
	WCHAR_T_CONV_VLA(lpsz);
	ret = func(lpsz_w);
	WCHAR_T_FREE(lpsz);
	return ret;
}
