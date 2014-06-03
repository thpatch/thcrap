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

DWORD WINAPI WrapGetString(
	__in WrapGetStringFunc_t func,
	__in DWORD nBufferLength,
	__out_ecount_part_opt(nBufferLength, return + 1) LPSTR lpBuffer
)
{
	DWORD ret;
	VLA(wchar_t, lpBuffer_w, nBufferLength);

	if(!lpBuffer) {
		VLA_FREE(lpBuffer_w);
	}
	ret = func(nBufferLength, lpBuffer_w);
	if(lpBuffer) {
		StringToUTF8(lpBuffer, lpBuffer_w, nBufferLength);
	} else {
		// Hey, let's be nice and return the _actual_ length.
		VLA(wchar_t, lpBufferReal_w, ret);
		func(ret, lpBufferReal_w);
		ret = StringToUTF8(NULL, lpBufferReal_w, 0);
		VLA_FREE(lpBufferReal_w);
	}
	VLA_FREE(lpBuffer_w);
	return ret;
}
