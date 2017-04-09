/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#pragma once

// Yes, I know this is the wrong way, but wineconsole...
extern int wine_flag;

// Writes [str] to a new file name [fn] in text mode.
int file_write_text(const char *fn, const char *str);

// Returns an array of patch selections. The engine's run configuration will
// contain all selected patches in a fully initialized state.
json_t* SelectPatchStack(json_t *repo_list);

json_t* SearchForGames(const char *dir, json_t *games_in);

// Shows a file write error and asks the user if they want to continue
int file_write_error(const char *fn);

int Ask(const char *question);
char* console_read(char *str, int n);
void cls(SHORT y);
void pause(void);

HRESULT CreateLink(
	const char *link_fn, const char *target_cmd, const char *target_args,
	const char *work_path, const char *icon_fn
);

json_t* ConfigureLocateGames(const char *games_js_path);
