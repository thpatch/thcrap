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
#define AlignUpToMultipleOf(val, mul) ((val) - ((val) % (mul)) + (mul))
#define AlignUpToMultipleOf2(val, mul) (((val) + (mul) - 1) & -(mul))

size_t dword_align(const size_t val);
BYTE* ptr_dword_align(const BYTE *in);
// Advances [src] by [num] and returns [num].
size_t ptr_advance(const unsigned char **src, size_t num);
// Copies [num] bytes from [src] to [dst] and advances [src].
size_t memcpy_advance_src(unsigned char *dst, const unsigned char **src, size_t num);

// Copies [num] bytes from [src] to [dst] and returns [dst] advanced by [num].
inline char* memcpy_advance_dst(char *dst, const void *src, size_t num)
{
	return ((char*)memcpy(dst, src, num)) + num;
}
/// --------

/// Strings
/// -------
#define PtrDiffStrlen(end_ptr, start_ptr) ((end_ptr) - (start_ptr))

inline char* strncpy_advance_dst(char *dst, const char *src, size_t len)
{
	assert(src);
	dst[len] = '\0';
	return memcpy_advance_dst(dst, src, len);
}

#ifdef __cplusplus
// Reference to a string somewhere else in memory with a given length.
// Extends std::string_view to add a json string constructor.
class stringref_t : public std::string_view {
public:
	stringref_t(const json_t* json) : std::string_view(json_string_value(json), json_string_length(json)) {}
	using std::string_view::string_view;
};

inline char* stringref_copy_advance_dst(char *dst, const stringref_t &strref)
{
	return strncpy_advance_dst(dst, strref.data(), strref.length());
}
#endif

// Replaces every occurence of the ASCII character [from] in [str] with [to].
inline void str_ascii_replace(char* str, const char from, const char to)
{
	char c;
	do {
		c = *str;
		if (c == from) *str = to;
		++str;
	} while (c);
}

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

// C23 compliant implementation of strndup
char* strndup(const char* source, size_t size);

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
  *	- Everything else is parsed as a hexadecimal or decimal number depending on
  *   whether hexadecimal digits are present.
  *
  * [ret] can be a nullptr if a potential parse error and/or a pointer to the
  * end of the parsed address are not needed.
  */
size_t str_address_value(const char *str, HMODULE hMod, str_address_ret_t *ret);

// Returns whether [c] is a valid hexadecimal character
bool is_valid_hex(char c);

// Returns either the hexadecimal value of [c]
// or -1 if [c] is not a valid hexadecimal character
int8_t hex_value(char c);

#ifdef __cplusplus

// Packs the bytes [c1], [c2], [c3], and [c4] together as a little endian integer
constexpr uint32_t TextInt(uint8_t c1, uint8_t c2 = 0, uint8_t c3 = 0, uint8_t c4 = 0) {
	return c4 << 24 | c3 << 16 | c2 << 8 | c1;
}

/// Geometry
/// --------
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
