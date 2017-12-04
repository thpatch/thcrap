/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Main include file.
  */

#pragma once

#define WIN32_LEAN_AND_MEAN

#ifdef THCRAP_EXPORTS
# define THCRAP_API __declspec(dllexport)
#else
# define THCRAP_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <win32_utf8.h>
#include <jansson.h>
#include "macros.h"
#include "exception.h"
#include "jansson_ex.h"
#include "global.h"
#include "log.h"
#include "patchfile.h"
#include "stack.h"
#include "binhack.h"
#include "breakpoint.h"
#include "util.h"
#include "mempatch.h"
#include "pe.h"
#include "plugin.h"
#include "strings.h"
#include "inject.h"
#include "init.h"
#include "jsondata.h"
#include "zip.h"
#include "bp_file.h"
#include "xpcompat.h"
#include "repo.h"
#include "search.h"
#include "shelllink.h"

#ifdef __cplusplus
}

/// defer implementation for C++
/// http://www.gingerbill.org/article/defer-in-cpp.html
/// ----------------------------
template <typename F> struct privDefer {
	F f;
	explicit privDefer(F f) : f(f) {}
	~privDefer() { f(); }
};

template <typename F> privDefer<F> defer_func(F f) {
	return privDefer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_func([&] () {code; })
/// ----------------------------
#endif
