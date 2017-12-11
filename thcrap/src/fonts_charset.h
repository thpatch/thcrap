/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Functions about fonts and character sets.
  */

#pragma once

int font_has_character(HDC hdc, WCHAR c);
HFONT font_create_for_character(const LOGFONTW *lplf, WORD c);
