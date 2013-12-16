/*
 * Touhou Community Reliant Automatic Patcher
 * DLL component
  *
  * ----
  *
  * Stuff related to text rendering.
 */

#pragma once

#define HAVE_TEXTDISP 1

void patch_fonts_load(const json_t *patch_info);

void textdisp_init(void);
int textdisp_detour(HMODULE hMod);
