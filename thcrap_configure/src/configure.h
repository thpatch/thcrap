/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#pragma once

#include <conio.h>

json_t* SelectPatchStack(json_t *server_js, json_t *patch_stack);
json_t* SearchForGames(const char *dir, json_t *games_in);

int Ask(const char *question);
void cls(SHORT y);

HRESULT CreateLink(
	const char *link_fn, const char *target_cmd, const char *target_args,
	const char *work_path, const char *icon_fn
);

json_t* ConfigureLocateGames(const char *games_js_path);
