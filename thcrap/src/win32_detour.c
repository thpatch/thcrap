/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Import Address Table detour calls for the win32_utf8 functions.
  */

#include "thcrap.h"

void win32_detour(void)
{
	const w32u8_dll_t *dll = w32u8_get_wrapped_functions();
	while(dll && dll->name) {
		detour_chain_w32u8(dll++);
	}
}
