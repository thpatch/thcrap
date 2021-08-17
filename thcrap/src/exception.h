/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Custom exception handler.
  */

#pragma once

TH_STATIC_ASSERT(sizeof(void*) <= 4, \
"Currently Win64 exception handling would cause hackpoints to crash " \
"and burn horribly. This is a solvable problem if the various formats " \
"of unwind information can be created at runtime, but this needs "\
"to be looked into."
);

void set_exception_detail(uint8_t detail_level);

void exception_init(void);
