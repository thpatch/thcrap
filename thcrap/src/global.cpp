/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Globals, compile-time constants and runconfig abstractions.
  */

#include "thcrap.h"

json_t* run_cfg = NULL;
json_t* global_cfg = NULL;

const char* PROJECT_NAME(void)
{
	return "Touhou Community Reliant Automatic Patcher";
}
const char* PROJECT_NAME_SHORT(void)
{
	return "thcrap";
}
const char* PROJECT_URL(void)
{
	return "https://www.thpatch.net/wiki/Touhou_Patch_Center:Download";
}
DWORD PROJECT_VERSION(void)
{
	return 0x20191005;
}
const char* PROJECT_VERSION_STRING(void)
{
	static char ver_str[11] = {0};
	if(!ver_str[0]) {
		str_hexdate_format(ver_str, PROJECT_VERSION());
	}
	return ver_str;
}
const char* PROJECT_BRANCH(void)
{
	return "stable";
}
json_t* runconfig_get(void)
{
	return run_cfg;
}
void runconfig_set(json_t *new_run_cfg)
{
	json_decref(run_cfg);
	run_cfg = json_incref(new_run_cfg);
}

const json_t *runconfig_title_get(void)
{
	const json_t *id = json_object_get(run_cfg, "game");
	const json_t *title = strings_get(json_string_value(id));
	if(!title) {
		title = json_object_get(run_cfg, "title");
	}
	return title ? title : (id ? id : NULL);
}

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

BOOL globalconfig_get_boolean(char* key, const BOOL default_value)
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

int globalconfig_set_boolean(char* key, const BOOL value)
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

long long globalconfig_get_integer(char* key, const long long default_value)
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

int globalconfig_set_integer(char* key, const long long value)
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
