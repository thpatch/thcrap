/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Basic long double support for MSVC.
  */

#include "thcrap.h"

#ifdef _MSC_VER

LongDouble80 ltold(int32_t _in) {
	LongDouble80 _out;
	__asm {
		FILD DWORD PTR[_in];
		FSTP TBYTE PTR[_out];
	}
	return _out;
}

LongDouble80 ultold(uint32_t _in) {
	union {
		struct {
			uint32_t dl;
			uint32_t dh;
		};
		int64_t q;
		LongDouble80 ld;
	} _out = { { _in, 0 } };
	__asm {
		FILD QWORD PTR[_out].q;
		FSTP TBYTE PTR[_out].ld;
	}
	return _out.ld;
}

LongDouble80 ftold(float _in) {
	LongDouble80 _out;
	__asm {
		FLD DWORD PTR[_in];
		FSTP TBYTE PTR[_out];
	}
	return _out;
}

LongDouble80 dtold(double _in) {
	LongDouble80 _out;
	__asm {
		FLD QWORD PTR[_in];
		FSTP TBYTE PTR[_out];
	}
	return _out;
}

int32_t ldtol(LongDouble80 _in) {
	__asm {
		FLD TBYTE PTR[_in];
		FISTP DWORD PTR[_in];
	}
	return *(int32_t*)&_in;
}

float ldtof(LongDouble80 _in) {
	__asm {
		FLD TBYTE PTR[_in];
		FSTP DWORD PTR[_in];
	}
	return *(float*)&_in;
}

double ldtod(LongDouble80 _in) {
	__asm {
		FLD TBYTE PTR[_in];
		FSTP QWORD PTR[_in];
	}
	return *(double*)&_in;
}

#else

LongDouble80 ltold(int32_t _in) {
	return (LongDouble80)_in;
}

LongDouble80 ultold(uint32_t _in) {
	return (LongDouble80)_in;
}

LongDouble80 ftold(float _in) {
	return (LongDouble80)_in;
}

LongDouble80 dtold(double _in) {
	return (LongDouble80)_in;
}

int32_t ldtol(LongDouble80 _in) {
	return (int32_t)_in;
}

float ldtof(LongDouble80 _in) {
	return (float)_in;
}

double ldtod(LongDouble80 _in) {
	return (double)_in;
}

#endif
