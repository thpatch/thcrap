/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Random utility functions.
  */

#pragma once
#include <uchar.h>

#define TH_CALLER_CLEANUP(func) TH_NODISCARD_REASON("Return value must be passed to '"#func"' by caller!")
#define TH_CALLER_FREE TH_CALLER_CLEANUP(free)
#define TH_CALLER_DELETE TH_CALLER_CLEANUP(delete)
#define TH_CALLER_DELETEA TH_CALLER_CLEANUP(delete[])
#define TH_CHECK_RET TH_NODISCARD_REASON("Return value must be checked to determine validity of other outputs!")

#define BoolStr(Boolean) (Boolean ? "true" : "false")

/*
Even when using C++20 [[unlikely]] attributes, MSVC prioritizes the first branch of
an if statement, which can screw up branch prediction in hot code.

Thus something like:	if (!pointer) [[unlikely]] return NULL;

Will *at best* be compiled to something like:

MOV EAX, pointer; TEST EAX, EAX; JZ SkipRet; RET;

The simplest workaround in hot code that needs a lot of conditions while remaining
readable is to invert the condition, make the first branch an empty statement, and put
the intended staements in an else block. However, this still looks extremely weird and
confusing, so this macro exists to better document the intent.
*/
#define unexpected(condition) (!(condition)) TH_LIKELY; TH_UNLIKELY else

/// Pointers
/// --------
#define AlignUpToMultipleOf(val, mul) ((val) - ((val) % (mul)) + (mul))
#define AlignUpToMultipleOf2(val, mul) (((val) + (mul) - 1) & -(mul))

#define dword_align(val) (size_t)AlignUpToMultipleOf2((size_t)(val), 4)
#define ptr_dword_align(val) (BYTE*)dword_align((uintptr_t)(val))

#if defined(_MSC_VER)
#ifdef TH_X64
#define CurrentImageBase (*(uintptr_t*)(__readgsqword(0x60) + 0x10))
#else
#define CurrentImageBase (*(uintptr_t*)(__readfsdword(0x30) + 0x8))
#endif
#else
#define CurrentImageBase ((uintptr_t)GetModuleHandle(NULL))
#endif

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

#ifdef __cplusplus
extern "C++" {

struct UniqueFree {
	inline void operator()(void* mem_to_free) {
		free(mem_to_free);
	}
};

template <typename T>
class unique_alloc : public std::unique_ptr<T, UniqueFree> {
public:
	typedef std::unique_ptr<T, UniqueFree> unique_ptr_base;
	using unique_ptr_base::unique_ptr_base;

	inline bool alloc_size(size_t size) noexcept {
		T* temp = (T*)malloc(size);
		bool success = (temp != NULL);
		this->reset(temp);
		return temp;
	}

	inline bool alloc_array(size_t count) noexcept {
		T* temp = (T*)malloc(count * sizeof(T));
		bool success = (temp != NULL);
		this->reset(temp);
		return temp;
	}

	inline bool resize(size_t size) noexcept {
		T* temp = (T*)realloc((void*)this->get(), size);
		bool success = (temp != NULL);
		if (success) {
			(void)this->release();
			this->reset(temp);
		}
		return success;
	}
};

template <typename T, std::enable_if_t<!std::is_array_v<T>, int> = 0>
inline unique_alloc<T> make_unique_alloc(size_t size) {
	return unique_alloc<T>((T*)malloc(size));
}

template <typename T, std::enable_if_t<std::is_array_v<T>&& std::extent_v<T> == 0, int> = 0>
inline unique_alloc<std::remove_extent_t<T>> make_unique_alloc(void) {
	return unique_alloc<std::remove_extent_t<T>>((std::decay_t<T>)malloc(sizeof(std::remove_extent_t<T>)));
}

template <typename T, std::enable_if_t<std::is_array_v<T> && std::extent_v<T> == 0, int> = 0>
inline unique_alloc<std::remove_extent_t<T>> make_unique_alloc(size_t count) {
	return unique_alloc<std::remove_extent_t<T>>((std::decay_t<T>)malloc(count * sizeof(std::remove_extent_t<T>)));
}

template <typename T, std::enable_if_t<std::is_array_v<T>&& std::extent_v<T> != 0, int> = 0>
inline unique_alloc<std::remove_extent_t<T>> make_unique_alloc(void) {
	return unique_alloc<std::remove_extent_t<T>>((std::decay_t<T>)malloc(std::extent_v<T> * sizeof(std::remove_extent_t<T>)));
}

template <typename T, std::enable_if_t<std::is_array_v<T>&& std::extent_v<T> != 0, int> = 0>
inline unique_alloc<std::remove_extent_t<T>> make_unique_alloc(size_t count) {
	return unique_alloc<std::remove_extent_t<T>>((std::decay_t<T>)malloc(std::extent_v<T> * count * sizeof(std::remove_extent_t<T>)));
}

}
#endif

// String to size_t
size_t TH_FORCEINLINE strtouz(const char* str, char** str_end, int base) {
#ifdef TH_X64
	return strtoull(str, str_end, base);
#else
	return strtoul(str, str_end, base);
#endif
}

int _vasprintf(char** buffer_ret, const char* format, va_list va);
int _asprintf(char** buffer_ret, const char* format, ...);

#define PtrDiffStrlen(end_ptr, start_ptr) ((size_t)((end_ptr) - (start_ptr)))

THCRAP_API TH_CALLER_FREE inline char16_t* utf8_to_utf16(const char* utf8_str) {
	if (utf8_str) {
		size_t utf8_len = strlen(utf8_str) + 1;
		char16_t* utf16_str = (char16_t*)malloc(utf8_len * sizeof(char16_t));
		mbstate_t state;
		memset(&state, 0, sizeof(state));
		char16_t* utf16_write = utf16_str - 1;
		size_t c;
		while (1) {
			switch (c = mbrtoc16(++utf16_write, utf8_str, utf8_len, &state)) {
				case 0:				return utf16_str;
				case -1: case -2:	goto BreakAll;
				case -3:			continue;
				default:			utf8_str += c; utf8_len -= c;
			}
		}
	BreakAll:
		free(utf16_str);
	}
	return NULL;
}

#include <intrin.h>

THCRAP_API TH_CALLER_FREE inline char16_t* utf8_to_utf16_bit_test_reset(const char* utf8_str) {
	if (utf8_str) {
		char16_t* utf16_str = (char16_t*)malloc((strlen(utf8_str) + 1) * sizeof(char16_t));
		char16_t* utf16_write = utf16_str;

		uint32_t codepoint = 0;
		int8_t multibyte_index = -1; // Signed = Single byte, Unsigned = Multibyte
		bool is_four_bytes = false;
		while (1) {
			uint32_t cur_byte = (uint8_t)*utf8_str++; // Zero Extend
			if (!_bittestandreset((long*)&cur_byte, 7)) {
				*utf16_write++ = cur_byte; // Single byte character
				if (cur_byte != '\0') continue;
				if (multibyte_index >= 0) break; // ERROR: Unfinished multi-byte sequence
				return utf16_str;
			}
			if (_bittestandreset((long*)&cur_byte, 6)) { // Leading byte
				if (multibyte_index >= 0) break; // ERROR: Leading byte before end of previous sequence
				multibyte_index = 1 + _bittestandreset((long*)&cur_byte, 5); // Test 3 byte bit
				if (multibyte_index == 2) {
					is_four_bytes = _bittestandreset((long*)&cur_byte, 4); // Test 4 byte bit
					if (is_four_bytes & (bool)_bittest((long*)&cur_byte, 3)) break; // ERROR: No 5 byte sequences
					multibyte_index += is_four_bytes;
				}
			}
			if (multibyte_index < 0) break; // ERROR: Trailing byte before leading byte
			codepoint |= cur_byte << 6 * (uint8_t)multibyte_index; // Accumulate to codepoint
			if (--multibyte_index < 0) { // Sequence is finished
				if (!is_four_bytes) {
					*utf16_write++ = codepoint;
					codepoint = 0;
				}
				else {
					is_four_bytes = false;
					*(uint32_t*)utf16_write = 0xDC00D800 | // Surrogate Base
						(codepoint & ~0x103FF) >> 10 | // High Surrogate
						(codepoint & 0x3FF) << 16; // Low Surrogate
					utf16_write += 2;
					codepoint = 0;
				}
			}
		}
		free(utf16_str);
	}
	return NULL;
}

THCRAP_API TH_CALLER_FREE inline char16_t* utf8_to_utf16_bit_scan(const char* utf8_str) {
	if (utf8_str) {
		char16_t* utf16_str = (char16_t*)malloc((strlen(utf8_str) + 1) * sizeof(char16_t));
		char16_t* utf16_write = utf16_str;

		uint32_t codepoint = 0;
		int8_t multibyte_index = -1; // Signed = Single byte, Unsigned = Multibyte
		bool is_four_bytes = false;
		while (1) {
			int8_t cur_byte = *utf8_str++;
			uint32_t pad_cur_byte = (uint8_t)cur_byte; // Zero Extend
			if (cur_byte >= 0) {
				*utf16_write++ = pad_cur_byte; // Single byte character
				if (cur_byte != '\0') continue;
				if (multibyte_index >= 0) break; // ERROR: Unfinished multi-byte sequence
				return utf16_str;
			}
			uint32_t BitScan = (uint8_t)~cur_byte; // Flip bits THEN Zero Extend
			(void)_BitScanReverse((unsigned long*)&BitScan, BitScan); // Count leading zero bits
			uint8_t BitIndex = 6 - BitScan; // Convert to multibyte index
			pad_cur_byte &= 0b01111111u >> BitIndex;
			if (BitIndex) { // Leading byte
				if (multibyte_index >= 0) break; // ERROR: Leading byte before end of previous sequence
				if (BitIndex > 3) break; // ERROR: No 5 byte sequences
				is_four_bytes = BitIndex == 3;
				multibyte_index = BitIndex;
			}
			if (multibyte_index < 0) break; // ERROR: Trailing byte before leading byte
			codepoint |= pad_cur_byte << 6 * (uint8_t)multibyte_index; // Accumulate to codepoint
			if (--multibyte_index < 0) { // Sequence is finished
				if (!is_four_bytes) {
					*utf16_write++ = codepoint;
					codepoint = 0;
				}
				else {
					is_four_bytes = false;
					*(uint32_t*)utf16_write = 0xDC00D800 | // Surrogate Base
						(codepoint & ~0x103FF) >> 10 | // High Surrogate
						(codepoint & 0x3FF) << 16; // Low Surrogate
					utf16_write += 2;
					codepoint = 0;
				}
			}
		}
		free(utf16_str);
	}
	return NULL;
}

THCRAP_API TH_CALLER_FREE inline char16_t* utf8_to_utf16_masking(const char* utf8_str) {
	if (utf8_str) {
		char16_t* utf16_str = (char16_t*)malloc((strlen(utf8_str) + 1) * sizeof(char16_t));
		char16_t* utf16_write = utf16_str;

		uint32_t temp;
#define BitMask(value, mask) \
		((bool)((value) & (mask)))
#define BitMaskAndReset(value, mask) \
		(temp = (value), temp != ((value) &= ~(mask)))
#define BitMaskAndSet(value, mask) \
		(temp = (value), temp != ((value) |= (mask)))
#define BitMaskAndComplement(value, mask) \
		(temp = (value), temp != ((value) ^= (mask)))

		uint32_t codepoint = 0;
		int8_t multibyte_index = -1; // Signed = Single byte, Unsigned = Multibyte
		bool is_four_bytes = false;
		while (1) {
			int32_t cur_byte = *utf8_str++; // Sign Extend
			if (cur_byte >= 0) {
				*utf16_write++ = cur_byte; // Single byte character
				if (cur_byte != '\0') continue;
				if (multibyte_index >= 0) break; // ERROR: Unfinished multibyte sequence
				return utf16_str;
			}
			cur_byte &= 0b01111111; // Get rid of the sign extended bits
			if (BitMaskAndReset(cur_byte, 0b01000000)) { // Leading byte
				if (multibyte_index >= 0) break;  // ERROR: Leading byte before end of previous sequence
				multibyte_index = 1 + BitMaskAndReset(cur_byte, 0b00100000); // Test 3 byte bit
				if (multibyte_index == 2) {
					is_four_bytes = BitMaskAndReset(cur_byte, 0b00010000); // Test 4 byte bit
					if (is_four_bytes & BitMask(cur_byte, 0b00001000)) break; // ERROR: No 5 byte sequences
					multibyte_index += is_four_bytes;
				}
			}
			if (multibyte_index < 0) break; // ERROR: Trailing byte before leading byte
			codepoint |= cur_byte << 6 * (uint8_t)multibyte_index; // Accumulate to codepoint
			if (--multibyte_index < 0) { // Sequence is finished
				if (!is_four_bytes) {
					*utf16_write++ = codepoint;
					codepoint = 0;
				}
				else {
					is_four_bytes = false;
					*(uint32_t*)utf16_write = 0xDC00D800 | // Surrogate Base
						(codepoint & ~0x103FF) >> 10 | // High Surrogate
						(codepoint & 0x3FF) << 16; // Low Surrogate
					utf16_write += 2;
					codepoint = 0;
				}
			}
		}
		free(utf16_str);
	}
	return NULL;
}

THCRAP_API TH_CALLER_FREE inline char32_t* utf8_to_utf32(const char* utf8_str) {
	if (utf8_str) {
		size_t utf8_len = strlen(utf8_str) + 1;
		char32_t* utf32_str = (char32_t*)malloc(utf8_len * sizeof(char32_t));
		mbstate_t state;
		memset(&state, 0, sizeof(state));
		char32_t* utf32_write = utf32_str - 1;
		size_t c;
		while (1) {
			switch (c = mbrtoc32(++utf32_write, utf8_str, utf8_len, &state)) {
				case 0:				return utf32_str;
				case -1: case -2:	goto BreakAll;
				case -3:			continue;
				default:			utf8_str += c; utf8_len -= c;
			}
		}
	BreakAll:
		free(utf32_str);
	}
	return NULL;
}

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
	inline stringref_t(const json_t* json) : std::string_view(json_string_value(json), json_string_length(json)) {}
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

/// -------

// Custom strndup variant that returns (size + 1) bytes.
inline TH_CALLER_FREE char* strdup_size(const char* src, size_t size) {
	char* ret = (char*)malloc(size + 1);
	if (!ret) return NULL;
	// strncpy will 0 pad
	if (!memccpy(ret, src, '\0', size)) {
		ret[size] = '\0';
	}
	return ret;
}

// C23 compliant implementation of strndup
// Allocates a buffer of (strnlen(s, size) + 1) bytes.
inline TH_CALLER_FREE char* strndup(const char* src, size_t size) {
	return strdup_size(src, strnlen(src, size));
}

#ifdef __cplusplus
extern "C++" {

// Custom strdup variants that efficiently concatenate 2-3 strings.
// Structured specifically so that the compiler can optimize the copy
// operations into MOVs for any string literals.

inline TH_CALLER_FREE char* strdup_cat(std::string_view str1, std::string_view str2) {
	const size_t total_size = str1.length() + str2.length();
	char* ret = (char*)malloc(total_size + 1);
	ret[total_size] = '\0';
	char* ret_temp = ret;
	ret_temp += str1.copy(ret_temp, str1.length());
	str2.copy(ret_temp, str2.length());
	return ret;
}

inline TH_CALLER_FREE char* strdup_cat(std::string_view str1, char sep, std::string_view str2) {
	const size_t total_size = str1.length() + sizeof(sep) + str2.length();
	char* ret = (char*)malloc(total_size + 1);
	ret[total_size] = '\0';
	char* ret_temp = ret;
	ret_temp += str1.copy(ret_temp, str1.length());
	*ret_temp++ = sep;
	str2.copy(ret_temp, str2.length());
	return ret;
}

inline TH_CALLER_FREE char* strdup_cat(std::string_view str1, std::string_view str2, std::string_view str3) {
	const size_t total_size = str1.length() + str2.length() + str3.length();
	char* ret = (char*)malloc(total_size + 1);
	ret[total_size] = '\0';
	char* ret_temp = ret;
	ret_temp += str1.copy(ret_temp, str1.length());
	ret_temp += str2.copy(ret_temp, str2.length());
	str3.copy(ret_temp, str3.length());
	return ret;
}

}
#endif

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
  * - "0b": Binary value
  * - "Rb": Binary version of "Rx".
  * - "0": Decimal, since accidental octal bugs are evil.
  * - "R": Decimal version of "Rx".
  *
  * [ret] can be a nullptr if a potential parse error and/or a pointer to the
  * end of the parsed address are not needed.
  */
size_t str_address_value(const char *str, HMODULE hMod, str_address_ret_t *ret);

// Returns whether [c] is a valid hexadecimal character
bool is_valid_hex(char c);

#define is_valid_decimal(c) ((uint8_t)((c) - '0') < 10)

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
