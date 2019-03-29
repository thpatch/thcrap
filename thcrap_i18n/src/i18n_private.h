#pragma once
#include <windows.h>
#include "thcrap_i18n.h"
#include <string>
#include <jansson.h>
// i18n.cpp
struct I18nCache;

// files.cpp
bool i18n_init_path();
json_t *i18n_load_json(const char *subdir, const char *file, size_t flags, json_error_t *error);
int i18n_dump_json(const json_t *json, const char *subdir, const char *file, size_t flags);
bool i18n_exists_json(const char *subdir, const char *file);

// plurals.cpp
unsigned _i18n_plural(unsigned long num);

// dllmain.cpp
extern HINSTANCE g_instance;
extern bool g_isEnabled;
#define IS_I18N_ENABLED() (g_isEnabled)

template<typename T, size_t index>
struct TLSProxy {

	static void **tls() {
		void **tls_init();
		extern DWORD g_tls;

		void **tls = (void**)TlsGetValue(g_tls);
		return tls ? tls : tls_init();
	}
	operator T() {
		return (T)tls()[index];
	}
	TLSProxy& operator=(const T&val) {
		tls()[index] = (void*)val;
		return *this;
	}
	T operator->() {
		return T(*this);
	}
};
extern TLSProxy<const char*, 0> lastdomain_raw;
extern TLSProxy<std::string*, 1> lastdomain;
extern TLSProxy<I18nCache*, 2> lastcache;
#define TLS_MAX_INDEX 3
