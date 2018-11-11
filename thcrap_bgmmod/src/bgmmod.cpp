/**
  * Touhou Community Reliant Automatic Patcher
  * BGM modding library for games using uncompressed PCM music
  *
  * ----
  *
  * Implementations of public functions not related to specific codecs.
  */

#include <thcrap.h>
#include "bgmmod.hpp"

/// String constants
/// ----------------
const stringref_t LOOP_INFIX = ".loop";
/// ----------------

/// Error reporting and debugging
/// -----------------------------
void bgmmod_verrorf(const char *text, va_list va)
{
	log_vmboxf("BGM modding error", MB_OK | MB_ICONERROR, text, va);
}

void bgmmod_errorf(const char *text, ...)
{
	va_list va;
	va_start(va, text);
	bgmmod_verrorf(text, va);
	va_end(va);
}
/// -----------------------------
