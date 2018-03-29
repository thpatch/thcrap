/**
  * Touhou Community Reliant Automatic Patcher
  * Update module
  *
  * ----
  *
  * Plugin setup
  */

extern "C" void http_mod_exit(void);

void thcrap_update_exit(void)
{
	http_mod_exit();
}
