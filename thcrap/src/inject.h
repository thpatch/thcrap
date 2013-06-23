/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * DLL injector.
  * Adapted from http://www.codeproject.com/Articles/20084/completeinject
  */

#pragma once

// Injects thcrap into the given [hProcess], and passes [setup_fn].
void thcrap_inject(HANDLE hProcess, const char *setup_fn);
