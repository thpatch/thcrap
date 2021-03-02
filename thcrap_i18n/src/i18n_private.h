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
bool i18n_exists_json(const char *subdir, const char *file);
int i18n_load_lang(char *buf, size_t len);
int i18n_save_lang(const char *buf);

// plurals.cpp
unsigned i18n_plural(unsigned long num);

// dllmain.cpp
extern HINSTANCE i18n_instance;
extern bool i18n_is_enabled;
#define IS_I18N_ENABLED() (i18n_is_enabled)

struct TLSBlock {
	const char *lastdomain_raw;
	std::string lastdomain;
	I18nCache *lastcache;
};
TLSBlock *i18n_tls_get();
