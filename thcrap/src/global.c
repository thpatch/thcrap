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

const char* PROJECT_NAME()
{
	return "Touhou Community Reliant Automatic Patcher";
}
const char* PROJECT_NAME_SHORT()
{
	return "thcrap";
}
const DWORD PROJECT_VERSION()
{
	return 0x20130805;
}
const char* PROJECT_VERSION_STRING()
{
	static char ver_str[11] = {0};
	if(!ver_str[0]) {
		str_hexdate_format(ver_str, PROJECT_VERSION());
	}
	return ver_str;
}
json_t* runconfig_get()
{
	return run_cfg;
}
void runconfig_set(json_t *new_run_cfg)
{
	run_cfg = new_run_cfg;
}

void* runconfig_func_get(const char *name)
{
	json_t *funcs = json_object_get(run_cfg, "funcs");
	return (void*)json_object_get_hex(funcs, name);
}
