/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Random utility functions.
  */

#pragma once

/// Pointers
/// --------
size_t dword_align(const size_t val);
BYTE* ptr_dword_align(const BYTE *in);
// Advances [src] by [num] and returns [num].
size_t ptr_advance(const unsigned char **src, size_t num);
// Copies [num] bytes from [src] to [dst] and advances [src].
size_t memcpy_advance_src(unsigned char *dst, const unsigned char **src, size_t num);

// Copies [num] bytes from [src] to [dst] and returns [dst] advanced by [num].
__inline char* memcpy_advance_dst(char *dst, const void *src, size_t num)
{
	memcpy(dst, src, num);
	return dst + num;
}
/// --------

/// Strings
/// -------
__inline char* strncpy_advance_dst(char *dst, const char *src, size_t len)
{
	assert(src);
	dst[len] = '\0';
	return memcpy_advance_dst(dst, src, len);
}

#ifdef __cplusplus
// Reference to a string somewhere else in memory with a given length.
// TODO: Rip out and change to std::string_view once C++17 is more widespread.
typedef struct stringref_t {
	const char *str;
	int len;

	// No default constructor = no potential uninitialized
	// string pointer = good

	stringref_t(const char *str) : str(str), len(strlen(str)) {}
	stringref_t(const char *str, int len) : str(str), len(len) {}

	stringref_t(const json_t *json)
		: str(json_string_value(json)), len(json_string_length(json)) {
	}
} stringref_t;

__inline char* stringref_copy_advance_dst(char *dst, const stringref_t &strref)
 {
	return strncpy_advance_dst(dst, strref.str, strref.len);
}
#endif

// Replaces every occurence of the ASCII character [from] in [str] with [to].
void str_ascii_replace(char *str, const char from, const char to);

// Changes directory slashes in [str] to '/'.
void str_slash_normalize(char *str);

/**
  * Usually, the normal version with Unix-style forward slashes should be
  * used, but there are some cases which require the Windows one:
  *
  * - PathRemoveFileSpec is the only shlwapi function not to work with forward
  *   slashes, effectively chopping a path down to just the drive specifier
  *   (PathRemoveFileSpecU works as expected)
  * - In what is probably the weirdest bug I've ever encountered, th07
  *   _REQUIRES_ its launch path to have '\' instead of '/' - otherwise it...
  *   disables vsync.
  *   Yes, out of all sensible bugs there could have been, it had to be _this_.
  *   Amazing.
  */
void str_slash_normalize_win(char *str);

// Counts the number of digits in [number]
unsigned int str_num_digits(int number);

// Returns the base of the number in [str].
int str_num_base(const char *str);

// Prints the hexadecimal [date] (0xYYYYMMDD) as YYYY-MM-DD to [format]
void str_hexdate_format(char format[11], uint32_t date);

// Creates a lowercase copy of [str].
#define STRLWR_DEC(str) \
	STRLEN_DEC(str); \
	VLA(char, str##_lower, str##_len);

#define STRLWR_CONV(str) \
	memcpy(str##_lower, str, str##_len); \
	strlwr(str##_lower); \

#define STRLWR_FREE(str) \
	VLA_FREE(str##_lower);
/// -------

#define STR_ADDRESS_ERROR_NONE 0
#define STR_ADDRESS_ERROR_OVERFLOW 0x1
#define STR_ADDRESS_ERROR_GARBAGE 0x2

typedef struct {
	// Points to the first character after the number
	const char *endptr;

	// Bitfield of the flags #defined above
	uint8_t error;
} str_address_ret_t;

/**
  * Returns the numeric value of a stringified address at the machine's word
  * size. The following string prefixes are supported:
  *
  *	- "0x": Hexadecimal, as expected.
  *	- "Rx": Hexadecimal value relative to the base address of the module given in hMod.
  *	        If hMod is NULL, the main module of the current process is used.
  *	- Everything else is parsed as a decimal number.
  *
  * [ret] can be a nullptr if a potential parse error and/or a pointer to the
  * end of the parsed address are not needed.
  */
size_t str_address_value(const char *str, HMODULE hMod, str_address_ret_t *ret);

/// Geometry
/// --------
#ifdef __cplusplus
struct vector2_t {
	union {
		struct {
			float x, y;
		};
		float c[2];
	};

	bool operator ==(const vector2_t &other) const {
		return (x == other.x) && (y == other.y);
	}

	bool operator !=(const vector2_t &other) const {
		return !(operator ==(other));
	}
};

struct vector3_t {
	float x, y, z;
};

// Hey, it's unique at least. "rect_t" would be a lot more ambiguous, with the
// RECT structure of the Windows API being left/top/right/bottom.
struct xywh_t {
	union {
		struct {
			float x, y, w, h;
		};
		float c[4];
	};

	xywh_t scaled_by(float units) const {
		auto unit2 = units * 2.0f;
		return { x - units, y - units, w + unit2, h + unit2 };
	}
};
#endif
