/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Shortcuts creation
  */

#pragma once

// Create a shortcut with the given parameters.
// It is assumed that CoInitialize has already been called.
HRESULT CreateLink(
	const char *link_fn, const char *target_cmd, const char *target_args,
	const char *work_path, const char *icon_fn
);

// Create shortcuts for the given games
int CreateShortcuts(const char *run_cfg_fn, json_t *games);
