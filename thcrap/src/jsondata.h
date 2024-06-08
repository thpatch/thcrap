/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * JSON data storage for modules.
  */

#pragma once

THCRAP_API void jsondata_add(const char *fn);
THCRAP_API void jsondata_game_add(const char *fn);

// Returns a borrowed reference to the JSON data for [fn].
THCRAP_API json_t* jsondata_get(const char *fn);
THCRAP_API json_t* jsondata_game_get(const char *fn);
