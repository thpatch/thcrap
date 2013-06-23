/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Plugin setup.
  */

#include <thcrap.h>
#include "thcrap_tsa.h"

int __stdcall thcrap_init_plugin(json_t *run_cfg)
{
	patchhook_register("msg*.dat", patch_msg);
	patchhook_register("*.msg", patch_msg);
	return 0;
}
