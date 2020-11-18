/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Main include file.
  */

#pragma once

#include <thcrap.h>

typedef enum {
	TH_NONE,

	// • nsml engine
	TH_MEGAMARI,

	// • Use a slightly different XORing
	// • Use cv2 instead of bmp
	TH_NSML,

	// • Support for more file formats
	TH105,

	// • Loads files from both th123 and th105
	TH123,

	// • th135 engine
	// • filename hash: uses hash = ch ^ 0x1000193 * hash
	// • spells: using data/csv/Item*.csv
	// • spells: data/csv/story/*/*.csv has columns for popularity
	TH135,

	// • filename hash: uses hash = (hash ^ ch) * 0x1000193
	// • XOR: uses an additional AUX parameter
	// • XOR: key compunents are multiplied by -1
	// • endings: the last line in the text may be on another line in the pl file, and it doesn't wait for an input.
	// • spells: using data/csv/spellcards/*.csv and data/system/char_select3/*/equip/*/000.png.csv
	TH145,

	// • bgm: add a 1 offset to the IDs
	TH155,
	TH_MARILEGA,

	// • th175 engine
	TH175,

	// Any future game without relevant changes
	TH_FUTURE,
} tasofro_game_t;

extern tasofro_game_t game_id;

#ifdef __cplusplus
extern "C" {
#endif

int __stdcall thcrap_plugin_init();
int nsml_init();
int th135_init();
int th175_init();

int patch_plaintext(void *file_inout, size_t size_out, size_t size_in, const char*, json_t *patch);

#ifdef __cplusplus
}
#endif
