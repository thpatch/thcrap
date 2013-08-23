/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Win32 dialog patching.
  */

#pragma once

// Returns a translated DLGTEMPLATE(EX), or NULL if no translation was applied.
DLGTEMPLATE* dialog_translate(HINSTANCE hInstance, LPCSTR lpTemplateName);

int dialog_patch(HMODULE hMod);
