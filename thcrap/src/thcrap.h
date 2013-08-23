/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Main include file.
  */

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shlwapi.h>
#include <win32_utf8.h>
#include <jansson.h>
#include "..\config.h"
#include "macros.h"
#include "jansson_ex.h"
#include "global.h"
#include "log.h"
#include "patchfile.h"
#include "breakpoint.h"
#include "strutil.h"
#include "mempatch.h"
#include "strings.h"
#include "init.h"
