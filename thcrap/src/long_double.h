/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Basic long double support for MSVC.
  */

#pragma once

#ifdef _MSC_VER
typedef _LDOUBLE LongDouble80;
#else
typedef long double LongDouble80;
#endif

LongDouble80 ltold(int32_t _in);
LongDouble80 ultold(uint32_t _in);
LongDouble80 ftold(float _in);
LongDouble80 dtold(double _in);
int32_t ldtol(LongDouble80 _in);
float ldtof(LongDouble80 _in);
double ldtod(LongDouble80 _in);
