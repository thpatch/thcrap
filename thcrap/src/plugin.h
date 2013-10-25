/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Plug-in handling.
  */

#pragma once

/**
  * int thcrap_init_plugin(json_t *run_cfg)
  *
  * Parameters
  * ----------
  *	json_t *run_cfg
  *		Run configuration containing game, version, and user settings,
  *		merged in this order
  *
  * Return value
  * ------------
  *	0 on success
  *	1 if plugin loading failed and the plugin should be removed
  *
  * Called directly after the plugin was loaded via LoadLibrary.
  * To be identified as such, every thcrap plugin must export this function.
  */
typedef int (__stdcall *thcrap_init_plugin_type)(json_t *run_config);

// Loads all thcrap plugins from the current directory.
int plugins_load(void);

int plugins_close(void);
