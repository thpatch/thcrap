/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Globals, compile-time constants and runconfig abstractions.
  */

#include "thcrap.h"

json_t* global_cfg = NULL;

#define QUOTE2(x) #x
#define QUOTE(x) QUOTE2(x)
constexpr uint32_t IntToVersion(uint32_t n)
{
	if (n < 10)
		return n;
	return IntToVersion(n / 10) * 16 + n % 10;
}
constexpr uint32_t DateToVersion(uint32_t y, uint32_t m, uint32_t d)
{
	return IntToVersion(y) * 0x10000 + IntToVersion(m) * 0x100 + IntToVersion(d);
}

const char PROJECT_NAME[] = "Touhou Community Reliant Automatic Patcher";
const char PROJECT_NAME_SHORT[] = "thcrap";
const char PROJECT_URL[] = "https://www.thpatch.net/wiki/Touhou_Patch_Center:Download";
constexpr uint32_t PROJECT_VERSION = DateToVersion(PROJECT_VERSION_Y, PROJECT_VERSION_M, PROJECT_VERSION_D);
const char PROJECT_VERSION_STRING[] = QUOTE(PROJECT_VERSION_Y) "-" QUOTE(PROJECT_VERSION_M) "-" QUOTE(PROJECT_VERSION_D);
const char PROJECT_BRANCH[] = "stable";

void globalconfig_init(void)
{
	json_decref(global_cfg);
	global_cfg = json_load_file_report("config/config.js");
	if (!global_cfg) {
		global_cfg = json_object();
	}
}

int globalconfig_dump(void)
{
	return json_dump_file(global_cfg, "config/config.js", JSON_INDENT(2) | JSON_SORT_KEYS);
}

BOOL globalconfig_get_boolean(const char* key, const BOOL default_value)
{
	if (!global_cfg) {
		globalconfig_init();
	}
	errno = 0;
	json_t* value_json = json_object_get(global_cfg, key);
	if (!value_json) {
		errno = ENOENT;
		return default_value;
	}
	return json_boolean_value(value_json);
}

int globalconfig_set_boolean(const char* key, const BOOL value)
{
	if (!global_cfg) {
		globalconfig_init();
	}
	json_t* j_value = json_boolean(value);
	if (json_equal(j_value, json_object_get(global_cfg, key))) {
		json_decref(j_value);
		return 0;
	}
	json_object_set_new(global_cfg, key, j_value);
	return globalconfig_dump();
}

long long globalconfig_get_integer(const char* key, const long long default_value)
{
	if (!global_cfg) {
		globalconfig_init();
	}
	errno = 0;
	json_t* value_json = json_object_get(global_cfg, key);
	if (!value_json) {
		errno = ENOENT;
		return default_value;
	}
	return json_integer_value(value_json);
}

int globalconfig_set_integer(const char* key, const long long value)
{
	if (!global_cfg) {
		globalconfig_init();
	}
	json_t* j_value = json_integer(value);
	if (json_equal(j_value, json_object_get(global_cfg, key))) {
		json_decref(j_value);
		return 0;
	}
	json_object_set_new(global_cfg, key, j_value);
	return globalconfig_dump();
}

void globalconfig_release(void)
{
	json_decref(global_cfg);
	global_cfg = json_incref(NULL);
}

void* TH_CDECL thcrap_alloc(size_t size) {
	return malloc(size);
}

void TH_CDECL thcrap_free(void *mem) {
	free(mem);
}
