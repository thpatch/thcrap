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
	SHTYPE_AUTO = 0,
	SHTYPE_SHORTCUT = 1,
	SHTYPE_WRAPPER_ABSPATH = 2,
	SHTYPE_WRAPPER_RELPATH = 3,
};

// Create shortcuts for the given games
THCRAP_API int CreateShortcuts(const char *run_cfg_fn, games_js_entry *games, enum ShortcutsDestination destination, enum ShortcutsType shortcut_type);

// Get the resource ID of the first icon group for the given module.
// If the return value is a resource name (IS_INTRESOURCE(iconGroupId) is false),
// it must be freed with thcrap_free();
THCRAP_API LPWSTR GetIconGroupResourceId(HMODULE hModule);
