/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * C runtime functions.
  */

#pragma once

_Check_return_ FILE * __cdecl fopen_u(
	_In_z_ const char * _Filename,
	_In_z_ const char * _Mode
);
#undef fopen
#define fopen fopen_u
