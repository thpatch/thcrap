/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Games search on disk
  */

#pragma once

// Search for games recognized by thcrap in the given directory.
// If [dir] is NULL or an empty string, it will search on the whole system.
// [games_in] contains the list of games already known before the search.
json_t* SearchForGames(const char *dir, json_t *games_in);
