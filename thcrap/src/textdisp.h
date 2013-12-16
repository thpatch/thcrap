/*
 * Touhou Community Reliant Automatic Patcher
 * DLL component
  *
  * ----
  *
  * Stuff related to text rendering.
 */

#pragma once

void patch_fonts_load(const json_t *patch_info);

int textdisp_detour(HMODULE hMod);
