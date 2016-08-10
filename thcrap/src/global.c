/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Globals, compile-time constants and runconfig abstractions.
  */

#include "thcrap.h"

CRITICAL_SECTION cs_file_access;
json_t* run_cfg = NULL;

const char* PROJECT_NAME(void)
{
	return "Touhou Community Reliant Automatic Patcher";
}
const char* PROJECT_NAME_SHORT(void)
{
	return "thcrap";
}
const DWORD PROJECT_VERSION(void)
{
	return 0x20160810;
}
const char* PROJECT_VERSION_STRING(void)
{
	static char ver_str[11] = {0};
	if(!ver_str[0]) {
		str_hexdate_format(ver_str, PROJECT_VERSION());
	}
	return ver_str;
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
