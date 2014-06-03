/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Win32 dialog patching.
  */

#pragma once

// Translates the Win32 dialog [lpTemplateName] in [hInstance].
// The patch stack is also searched for a binary replacement resource
// ("<game>/dialog_[lpTemplateName].bin") to be used instead of the original.
// Returns a translated DLGTEMPLATE(EX), or NULL if no translation was applied.
DLGTEMPLATE* dialog_translate(HINSTANCE hInstance, LPCSTR lpTemplateName);

void dialog_mod_detour(void);
