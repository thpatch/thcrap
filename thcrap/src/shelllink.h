/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Shortcuts creation
  */

#pragma once

enum ShortcutsDestination
{
	SHDESTINATION_THCRAP_DIR = 1,
	SHDESTINATION_DESKTOP = 2,
	SHDESTINATION_START_MENU = 3,
	SHDESTINATION_GAMES_DIRECTORY = 4,
};

enum ShortcutsType
{
	SHTYPE_SHORTCUT = 1,
	SHTYPE_WRAPPER_ABSPATH = 2,
	SHTYPE_WRAPPER_RELPATH = 3,
};

// Create a shortcut with the given parameters.
// It is assumed that CoInitialize has already been called.
HRESULT CreateLink(
	const char *link_fn, const char *target_cmd, const char *target_args,
	const char *work_path, const char *icon_fn
);

// Create shortcuts for the given games
int CreateShortcuts(const char *run_cfg_fn, games_js_entry *games, enum ShortcutsDestination destination, enum ShortcutsType shortcut_type);
