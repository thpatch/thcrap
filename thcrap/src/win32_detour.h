/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Import Address Table detour calls for the win32_utf8 functions.
  */

#pragma once

// *Not* a _mod_ function to ensure that this remains on the lowest level
void win32_detour(void);
