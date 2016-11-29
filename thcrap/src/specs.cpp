/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Specifications for various versions of file formats used by a game.
  */

#include "thcrap.h"

void specs_mod_init(void)
{
	jsondata_add("formats.js");
}

json_t* specs_get(const char *format)
{
	json_t *specs = jsondata_get("formats.js");
	json_t *game_specs = json_object_get(runconfig_get(), "formats");
	const char *spec_id = json_object_get_string(game_specs, format);
	return json_object_get(specs, spec_id);
}
