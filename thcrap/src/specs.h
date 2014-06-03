/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Specifications for various versions of file formats used by a game.
  */

void specs_mod_init(void);

// Returns the format specification for [format].
json_t* specs_get(const char *format);
