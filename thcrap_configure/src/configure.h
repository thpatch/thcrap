/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#pragma once

/// Patch selection format: ["repo_id", "patch_id"]

// Bootstraps the patch selection [sel] by building a patch object
// with an archive directory and storing patch.js from [repo_servers] in it.
json_t* BootstrapPatch(const json_t *sel, json_t *repo_servers);

// Returns an array of patch selections.
json_t* SelectPatchStack(json_t *repo_js);

json_t* SearchForGames(const char *dir, json_t *games_in);

int Ask(const char *question);
void cls(SHORT y);
void pause(void);

HRESULT CreateLink(
	const char *link_fn, const char *target_cmd, const char *target_args,
	const char *work_path, const char *icon_fn
);

json_t* ConfigureLocateGames(const char *games_js_path);
