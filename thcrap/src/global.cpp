/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Globals, compile-time constants and runconfig abstractions.
  */

#include "thcrap.h"
#include <functional>

json_t* global_cfg = NULL;

#define QUOTE2(x) #x
#define QUOTE(x) QUOTE2(x)
constexpr uint32_t DateToVersion(uint32_t y, uint32_t m, uint32_t d)
{
	return y * 0x10000 + m * 0x100 + d;
}

#define CONCAT(x, y) x ## y
#define TO_HEX(a) CONCAT(0x, a)

const char PROJECT_NAME[] = "Touhou Community Reliant Automatic Patcher";
const char PROJECT_NAME_SHORT[] = "thcrap";
const char PROJECT_URL[] = "https://www.thpatch.net/wiki/Touhou_Patch_Center:Download";
constexpr uint32_t PROJECT_VERSION = DateToVersion(TO_HEX(PROJECT_VERSION_Y), TO_HEX(PROJECT_VERSION_M), TO_HEX(PROJECT_VERSION_D));
const char PROJECT_VERSION_STRING[] = QUOTE(PROJECT_VERSION_Y) "-" QUOTE(PROJECT_VERSION_M) "-" QUOTE(PROJECT_VERSION_D);
const char PROJECT_BRANCH[] = BUILD_PROJECT_BRANCH;

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

template<typename T, typename L>
T globalconfig_get(const L& json_T_value, const char* key, const T default_value)
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
	return json_T_value(value_json);
}

template<typename T, typename L>
int globalconfig_set(const L& json_T, const char* key, const T value)
{
	if (!global_cfg) {
		globalconfig_init();
	}
	json_t* j_value = json_T(value);
	if (json_equal(j_value, json_object_get(global_cfg, key))) {
		json_decref(j_value);
		return 0;
	}
	json_object_set_new(global_cfg, key, j_value);
	return globalconfig_dump();
}

BOOL globalconfig_get_boolean(const char* key, const BOOL default_value)
{
	return globalconfig_get<BOOL>(
		[](const json_t *value) { return json_boolean_value(value); },
		key, default_value);
}

int globalconfig_set_boolean(const char* key, const BOOL value)
{
	return globalconfig_set<BOOL>(
		[](const BOOL value) { return json_boolean(value); },
		key, value);
}

long long globalconfig_get_integer(const char* key, const long long default_value)
{
	return globalconfig_get<long long>(json_integer_value, key, default_value);
}

int globalconfig_set_integer(const char* key, const long long value)
{
	return globalconfig_set<long long>(json_integer, key, value);
}

const char *globalconfig_get_string(const char* key, const char *default_value)
{
	return globalconfig_get<const char*>(json_string_value, key, default_value);
}

int globalconfig_set_string(const char* key, const char *value)
{
	return globalconfig_set<const char*>(json_string, key, value);
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
