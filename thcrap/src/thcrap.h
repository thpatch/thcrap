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

#include "compiler_support.h"

#ifdef _M_X64
#define TH_X64
#endif

#ifdef THCRAP_EXPORTS
# define THCRAP_API TH_EXPORT
#else
# define THCRAP_API TH_IMPORT
#endif

#ifdef __cplusplus
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#endif

#include <win32_utf8.h>
// The bundled CRT includes a standards compliant snprintf,
// so the old macro definition to _snprintf is obsolete.
#undef snprintf
#include <stdbool.h>
#include <jansson.h>
#include "exception.h"
#include "long_double.h"
#include "util.h"
#include "jansson_ex.h"
#include "expression.h"
#include "global.h"
#include "runconfig.h"
#include "log.h"
#include "patchfile.h"
#include "stack.h"
#include "binhack.h"
#include "breakpoint.h"
#include "mempatch.h"
#include "pe.h"
#include "plugin.h"
#include "strings.h"
#include "strings_array.h"
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

#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))

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

// Automatically decref a json when leaving the current scope
class ScopedJson
{
private:
    json_t *obj = nullptr;

    void clear()
    {
        if (this->obj) {
            json_decref(this->obj);
        }
        this->obj = nullptr;
    }

    void assign(json_t *new_obj)
    {
        this->clear();
        if (new_obj) {
            this->obj = json_incref(new_obj);
        }
    }

    void steal(json_t *new_obj)
    {
        this->clear();
        this->obj = new_obj;
    }

public:
    ScopedJson()
    {}

    ScopedJson(json_t *obj)
    {
        this->steal(obj);
    }

    ScopedJson(const ScopedJson& src)
    {
        this->assign(src.obj);
    }
    ScopedJson& operator=(json_t *obj)
    {
        this->steal(obj);
        return *this;
    }
    ScopedJson& operator=(const ScopedJson& src)
    {
        this->assign(src.obj);
        return *this;
    }
    ScopedJson(ScopedJson&& src)
    {
        this->steal(src.obj);
        src.obj = nullptr;
    }
    ScopedJson& operator=(ScopedJson&& src)
    {
        this->steal(src.obj);
        src.obj = nullptr;
        return *this;
    }

    ~ScopedJson()
    {
        this->clear();
    }

    json_t *operator*() const
    {
        return this->obj;
    }
    operator bool()
    {
        return this->obj != nullptr;
    }
};
#endif
