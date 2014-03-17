/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * Main include file.
  */

#pragma once

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <psapi.h>
#include <stdio.h>

#include "macros.h"
#include "utf.h"

#include "gdi32_dll.h"
#include "kernel32_dll.h"
#include "msvcrt_dll.h"
#include "psapi_dll.h"
#include "shlwapi_dll.h"
#include "user32_dll.h"

// Sets a custom codepage for wide char conversion, which is used if the input
// to a *U function is not valid UTF-8.
// Useful for console applications (which use CP_OEMCP by default) or patching
// applications where the application's native codepage isn't ASCII.
void w32u8_set_fallback_codepage(UINT codepage);
