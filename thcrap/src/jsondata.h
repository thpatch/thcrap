/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * JSON data storage for modules.
  */

#pragma once

int jsondata_add(const char *fn);
int jsondata_game_add(const char *fn);

// Returns a borrowed reference to the JSON data for [fn].
json_t* jsondata_get(const char *fn);
json_t* jsondata_game_get(const char *fn);

void jsondata_mod_repatch(const json_t *files_changed);
void jsondata_mod_exit(void);
