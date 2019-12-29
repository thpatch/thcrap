/**
  * Touhou Community Reliant Automatic Patcher
  * Update module
  *
  * ----
  *
  * Update the patches before running the game
  */

#pragma once

#include <thcrap.h>

#ifdef __cplusplus
extern "C" {
#endif

// [game_id_fallback] is used to enforce updating this game's patch files in
// case [exe_fn] can't be identified as a known game.
// (Workaround for issue #69, https://github.com/thpatch/thcrap/issues/69,
// covering the standard use case of thcrap_loader being started with
// references to games.js IDs that are identical to those used in patches.)
BOOL loader_update_with_UI(const char *exe_fn, char *args, const char *game_id_fallback);

#ifdef __cplusplus
}
#endif
