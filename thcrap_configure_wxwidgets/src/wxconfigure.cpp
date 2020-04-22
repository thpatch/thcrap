/**
  * Touhou Community Reliant Automatic Patcher
  * wxWidgets-based patch stack configuration tool
  */

#include <thcrap.h>
#include "wxconfigure.h"

#include <win32_utf8/entry_winmain.c>
#include "thcrap_i18n/src/thcrap_i18n.h"
int __cdecl win32_utf8_main(int argc, const char *argv[])
{
	i18n_lang_init(THCRAP_I18N_APPDOMAIN);
    log_mbox(nullptr, MB_OK, "Hello world!");
	
	return 0;
}
