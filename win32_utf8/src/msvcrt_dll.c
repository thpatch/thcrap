/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * C runtime functions.
  */

#include "win32_utf8.h"

// Yes, this should better be implemented as a wrapper around fopen_s() (and
// thus, _wfopen_s()), but XP's msvcrt.dll doesn't have that function.
_Check_return_ FILE * __cdecl fopen_u(
	_In_z_ const char * _Filename,
	_In_z_ const char * _Mode
)
{
	FILE *ret = NULL;
	WCHAR_T_DEC(_Filename);
	WCHAR_T_DEC(_Mode);
	WCHAR_T_CONV(_Filename);
	WCHAR_T_CONV(_Mode);
	ret = _wfopen(_Filename_w, _Mode_w);
	WCHAR_T_FREE(_Filename);
	WCHAR_T_FREE(_Mode);
	return ret;
}
