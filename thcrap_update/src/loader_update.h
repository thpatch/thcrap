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

BOOL loader_update_with_UI(const char *exe_fn, char *args);

#ifdef __cplusplus
}
#endif
