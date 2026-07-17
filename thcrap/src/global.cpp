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

// Uncomment this to test updating from a dev version
/*
#undef PROJECT_VERSION_Y
#define PROJECT_VERSION_Y 0001
#undef PROJECT_VERSION_M
#define PROJECT_VERSION_M 01
#undef PROJECT_VERSION_D
#define PROJECT_VERSION_D 01
*/

static json_t* global_cfg = NULL;

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

enum ConfigVersion {
	OLD_CONFIG = 0,
	CONFIG_V1 = 1,
};

#if ENABLE_OVERHAULED_UPDATE_SETTINGS
#define CONFIG_VERSION_CURRENT CONFIG_V1
#else
#define CONFIG_VERSION_CURRENT OLD_CONFIG
#endif

int globalconfig_dump(json_t* cfg)
{
	return json_dump_file(cfg, "config/config.js", JSON_INDENT(2) | JSON_SORT_KEYS);
}

static json_t* globalconfig_init(void)
{
	json_decref(global_cfg);
	json_t* cfg = json_load_file_report("config/config.js");
	bool write_to_disk = false;
	if (cfg) {
#if ENABLE_OVERHAULED_UPDATE_SETTINGS
		// Config migrations
		json_t* version_j = json_object_get(cfg, "config_version");
		ConfigVersion version = version_j ? (ConfigVersion)json_integer_value(version_j) : OLD_CONFIG;
		if (version != CONFIG_VERSION_CURRENT) {
			switch (version) {
				case OLD_CONFIG: {
					update_type_t update_type;
					loader_type_t loader_type;
					if (json_is_true(json_object_get(cfg, "background_updates"))) {
						update_type = UPDATE_IN_BACKGROUND;
						loader_type = LOADER_PERSISTENT;
					}
					else if (json_is_true(json_object_get(cfg, "update_at_exit"))) {
						update_type = UPDATE_AT_EXIT;
						loader_type = LOADER_STARTUP_ONLY;
					}
					else {
						update_type = UPDATE_AT_STARTUP;
						loader_type = LOADER_STARTUP_ONLY;
					}
					json_object_set_new_nocheck(cfg, "update_type", json_integer(update_type));
					json_object_set_new_nocheck(cfg, "loader_type", json_integer(loader_type));
					json_object_del(cfg, "background_updates");
					json_object_del(cfg, "update_at_exit");
				}
			}
			json_object_set_new_nocheck(cfg, "config_version", json_integer(CONFIG_VERSION_CURRENT));
			MoveFileExW(L"config/config.js", L"config/config_old.js", MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);
			write_to_disk = true;
		}
#endif
	}
	else {
		cfg = json_object();
#if ENABLE_OVERHAULED_UPDATE_SETTINGS
		json_object_set_new_nocheck(cfg, "update_type", json_integer(DEFAULT_UPDATE_TYPE));
		json_object_set_new_nocheck(cfg, "loader_type", json_integer(DEFAULT_LOADER_TYPE));
		json_object_set_new_nocheck(cfg, "config_version", json_integer(CONFIG_V1));
#endif
		write_to_disk = true;
	}
	if (write_to_disk) {
		globalconfig_dump(cfg);
	}
	global_cfg = cfg;
	return cfg;
}

template<typename T, typename L>
inline T globalconfig_get(const L& json_T_value, const char* key, const T default_value)
{
	json_t* cfg = global_cfg;
	if unexpected(!cfg) {
		cfg = globalconfig_init();
	}
	json_t* value_json = json_object_get(cfg, key);
	if (!value_json) {
		return default_value;
	}
	return json_T_value(value_json);
}

template<typename T, typename L>
int globalconfig_set(const L& json_T, const char* key, const T value)
{
	json_t* cfg = global_cfg;
	if unexpected(!cfg) {
		cfg = globalconfig_init();
	}
	json_t* j_value = json_T(value);
	if (json_equal(j_value, json_object_get(cfg, key))) {
		json_decref(j_value);
		return 0;
	}
	json_object_set_new(cfg, key, j_value);
	return globalconfig_dump(cfg);
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
	global_cfg = NULL;
}

void* TH_CDECL thcrap_alloc(size_t size) {
	return malloc(size);
}

void TH_CDECL thcrap_free(void *mem) {
	free(mem);
}
