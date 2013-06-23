/*
 * Touhou Community Reliant Automatic Patcher
 * DLL component
  *
  * ----
  *
  * Stuff related to text rendering.
 */

#pragma once

// Calculates the rendered length of [str] on the current text DC
// with the currently selected font
DWORD __stdcall GetTextExtent(const char *str);

void patch_fonts_load(const json_t *patch_info, const json_t *patch_js);

int textdisp_init(HMODULE hMod);
int textdisp_patch(HMODULE hMod);
void textdisp_exit();
