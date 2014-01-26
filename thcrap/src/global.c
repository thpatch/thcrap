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
	return 0x20140126;
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

void* runconfig_func_get(const char *name)
{
	json_t *funcs = json_object_get(run_cfg, "funcs");
	return (void*)json_object_get_hex(funcs, name);
}

json_t* runconfig_format_get(const char *format)
{
	return json_object_get(json_object_get(run_cfg, "formats"), format);
}
