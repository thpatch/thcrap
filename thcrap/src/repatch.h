/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Hot-repatching.
  */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void repatch_mod_init(void*);
void repatch_mod_exit(void*);

#ifdef __cplusplus
}
#endif
