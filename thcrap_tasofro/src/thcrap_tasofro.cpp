/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Plugin entry point.
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"

tasofro_game_t game_id;

// Translate strings to IDs.
static tasofro_game_t game_id_from_string(const char *game)
{
	if (game == NULL) {
		return TH_NONE;
	}
	else if (!strcmp(game, "megamari")) {
		return TH_MEGAMARI;
	}
	else if (!strcmp(game, "nsml")) {
		return TH_NSML;
	}
	else if (!strcmp(game, "th105")) {
		return TH105;
	}
	else if (!strcmp(game, "th123")) {
		return TH123;
	}
	else if (!strcmp(game, "th135")) {
		return TH135;
	}
	else if (!strcmp(game, "th145")) {
		return TH145;
	}
	else if (!strcmp(game, "th155")) {
		return TH155;
	}
	else if (!strcmp(game, "marilega")) {
		return TH_MARILEGA;
	}
	else if (!strcmp(game, "th175")) {
		return TH175;
	}
	return TH_FUTURE;
}

extern "C" __declspec(dllexport) const char* steam_appid(void)
{
	switch(game_id) {
	case TH155:
		return "716710";
	default: // -Wswitch...
		return nullptr;
	}
}

int __stdcall thcrap_plugin_init()
{
	int base_tasofro_removed = stack_remove_if_unneeded("base_tasofro");
	if (base_tasofro_removed == 1) {
		return 1;
	}

	const char *game = runconfig_game_get();
	game_id = game_id_from_string(game);

	if(base_tasofro_removed == -1) {
		if(game_id == TH145) {
			log_mboxf(NULL, MB_OK | MB_ICONINFORMATION,
				"Support for TH14.5 has been moved out of the sandbox.\n"
				"\n"
				"Please reconfigure your patch stack; otherwise, you might "
				"encounter errors or missing functionality after further "
				"updates.\n"
			);
		} else {
			return 1;
		}
	}

	if (game_id >= TH175) {
		return th175_init();
	}
	else if (game_id >= TH135) {
		return th135_init();
	}
	else {
		return nsml_init();
	}

	return 0;
}
