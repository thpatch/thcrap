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
#include <string>

extern "C" {
#endif

#include <win32_utf8.h>
#include <jansson.h>
#include "exception.h"
#include "util.h"
#include "jansson_ex.h"
#include "global.h"
#include "log.h"
#include "patchfile.h"
#include "stack.h"
#include "binhack.h"
#include "breakpoint.h"
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
#include "fonts_charset.h"

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

#define SAFE_DELETE(x)       SAFE_CLEANUP(delete,   x);
#define SAFE_DELETE_ARRAY(x) SAFE_CLEANUP(delete[], x);

// Rust-style Option type. Useful for cases where the zero value of T is
// equally valid.
template <typename T> struct Option {
protected:
	bool valid;
	T val;

public:
	Option(T val) : valid(true), val(val) {}
	Option() : valid(false) {}

	bool is_none() {
		return !valid;
	}

	bool is_some() {
		return valid;
	}

	const T& unwrap() {
		assert(valid);
		return val;
	}

	const T& unwrap_or(const T &def) {
		return valid ? val : def;
	}
};
#endif
