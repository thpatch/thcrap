/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Functions about fonts and character sets.
  */

#pragma once

THCRAP_API int font_has_character(HDC hdc, WCHAR c);
THCRAP_API HFONT font_create_for_character(const LOGFONTW *lplf, WORD c);
