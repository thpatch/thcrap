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
#define TH_CALLER_CLOSE_HANDLE TH_CALLER_CLEANUP(CloseHandle)
#define TH_CHECK_RET TH_NODISCARD_REASON("Return value must be checked to determine validity of other outputs!")

#define TH_DEPRECATED_EXPORT TH_DEPRECATED_REASON("Exported function is only kept for backwards compatibility") THCRAP_API

#define BoolStr(Boolean) ((Boolean) ? "true" : "false")

#define member_size(type, member) sizeof(((type *)0)->member)

/*
Even when using C++20 [[unlikely]] attributes, MSVC prioritizes the first branch of
an if statement, which can screw up branch prediction in hot code.

Thus something like:	if (!pointer) [[unlikely]] return NULL;

Will *at best* be compiled to something like:

MOV EAX, pointer; TEST EAX, EAX; JZ SkipRet; RET;

The simplest workaround in hot code that needs a lot of conditions while remaining
readable is to invert the condition, make the first branch an empty statement, and put
the intended statements in an else block. However, this still looks extremely weird and
confusing, so this macro exists to better document the intent.
*/
#define unexpected(condition) (!(condition)) TH_LIKELY; else TH_UNLIKELY

#define func_ptr_typedef(return_type, calling_convention, name) \
typedef return_type (calling_convention* name)


/// Internal Windows Structs
/// -------
#include "ntdll.h"

#define CurrentImageBase ((uintptr_t)CurrentPeb()->ImageBaseAddress)

#define CurrentModuleHandle ((HMODULE)CurrentImageBase)

// TODO: Look into when this member gets used.
// Supposedly it's just scratch space for strings.
#define ReadTempUTF8Buffer(i) ((char)read_teb_member(StaticUTF8Buffer[(i)]))
#define WriteTempUTF8Buffer(i, c) (write_teb_member(StaticUTF8Buffer[(i)], (c)))

/// --------

/// Pointers
/// --------
#define AlignUpToMultipleOf(val, mul) ((val) - ((val) % (mul)) + (mul))
#define AlignUpToMultipleOf2(val, mul) (((val) + (mul) - 1) & -(mul))

#define dword_align(val) (size_t)AlignUpToMultipleOf2((size_t)(val), 4)
#define ptr_dword_align(val) (BYTE*)dword_align((uintptr_t)(val))

// Advances [src] by [num] and returns [num].
inline size_t ptr_advance(const unsigned char **src, size_t num) {
	*src += num;
	return num;
}
// Copies [num] bytes from [src] to [dst] and advances [src].
inline size_t memcpy_advance_src(unsigned char *dst, const unsigned char **src, size_t num) {
	memcpy(dst, *src, num);
	return ptr_advance(src, num);
}

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

#include <memory>

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
static size_t TH_FORCEINLINE strtouz(const char* str, char** str_end, int base) {
#ifdef TH_X64
	return strtoull(str, str_end, base);
#else
	return strtoul(str, str_end, base);
#endif
}

#define size_t_neg_one (~(size_t)0)

int _vasprintf(char** buffer_ret, const char* format, va_list va);
int _asprintf(char** buffer_ret, const char* format, ...);

#define PtrDiffStrlen(end_ptr, start_ptr) ((size_t)((end_ptr) - (start_ptr)))

#include <intrin.h>

TH_CALLER_FREE THCRAP_EXPORT_API inline char16_t* utf8_to_utf16(const char* utf8_str) {
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

TH_CALLER_FREE THCRAP_EXPORT_API inline char32_t* utf8_to_utf32(const char* utf8_str) {
	if (utf8_str) {
		char32_t* utf32_str = (char32_t*)malloc((strlen(utf8_str) + 1) * sizeof(char32_t));
		char32_t* utf32_write = utf32_str;

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
				*utf32_write++ = cur_byte; // Single byte character
				if (cur_byte != '\0') continue;
				if (multibyte_index >= 0) break; // ERROR: Unfinished multibyte sequence
				return utf32_str;
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
				*utf32_write++ = codepoint;
				codepoint = 0;
			}
		}
		free(utf32_str);
	}
	return NULL;
}

inline bool bittest32(uint32_t value, uint32_t bit) {
	return value & 1u << bit;
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

extern "C++" {
// Remove these once we support C++20
template<typename T>
static constexpr inline bool string_view_ends_with(const std::basic_string_view<T>& view, const std::basic_string_view<T>& compare) {
	return view.length() >= compare.length() && !memcmp(view.data() + view.length() - compare.length(), compare.data(), compare.length() * sizeof(T));
}
template<typename T, size_t N>
static constexpr inline bool string_view_ends_with(const std::basic_string_view<T>& view, const T(&compare)[N]) {
	return view.length() >= (N - 1) && !memcmp(view.data() + view.length() - (N - 1), compare, sizeof(T[N - 1]));
}
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

inline void wstr_ascii_replace(wchar_t* str, const wchar_t from, const wchar_t to)
{
	wchar_t c;
	do {
		c = *str;
		if (c == from) *str = to;
		++str;
	} while (c);
}

// Changes directory slashes in [str] to '/'.
TH_DEPRECATED_EXPORT void (str_slash_normalize)(char *str);

inline void str_slash_normalize_inline(char* str) {
	str_ascii_replace(str, '\\', '/');
}

#define str_slash_normalize(str) str_slash_normalize_inline(str)

inline void wstr_slash_normalize(wchar_t* str) {
	wstr_ascii_replace(str, L'\\', L'/');
}

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
TH_DEPRECATED_EXPORT void (str_slash_normalize_win)(char *str);

inline void str_slash_normalize_win_inline(char* str) {
	str_ascii_replace(str, '/', '\\');
}

#define str_slash_normalize_win(str) str_slash_normalize_win_inline(str)

inline void wstr_slash_normalize_win(wchar_t* str) {
	wstr_ascii_replace(str, L'/', L'\\');
}

// Counts the number of digits in [number]
TH_DEPRECATED_EXPORT unsigned int (str_num_digits)(int number);

inline unsigned int str_num_digits_inline(int number) {
	unsigned int digits = 0;
	if (number < 0) {
		digits = 1; // remove this line if '-' counts as a digit
	}
	while (number) {
		number /= 10;
		digits++;
	}
	return digits;
}

#define str_num_digits(number) str_num_digits_inline(number)

// Returns the base of the number in [str].
TH_DEPRECATED_EXPORT int (str_num_base)(const char *str);

inline int str_num_base_inline(const char* str) {
	return (str[0] == '0' && (str[1] | 0x20) == 'x') ? 16 : 10;
}

#define str_num_base(str) str_num_base_inline(str)

// Prints the hexadecimal [date] (0xYYYYMMDD) as YYYY-MM-DD to [buffer]
TH_DEPRECATED_EXPORT void (str_hexdate_format)(char buffer[11], uint32_t date);

inline void str_hexdate_format_inline(char buffer[11], uint32_t date) {
	sprintf(buffer, "%04x-%02x-%02x",
		(date & 0xffff0000) >> 16,
		(date & 0x0000ff00) >> 8,
		(date & 0x000000ff)
	);
}

#define str_hexdate_format(buffer, date) str_hexdate_format_inline((buffer),(date))

/// -------

// Custom strndup variant that returns (size + 1) bytes.
//
// Note: This needs to be marked extern when
// compiling as C to avoid linker errors
TH_CALLER_FREE extern inline char* strdup_size(const char* src, size_t size) {
	char* ret = (char*)malloc(size + 1);
	// strncpy will 0 pad
	if (ret && !memccpy(ret, src, '\0', size)) {
		ret[size] = '\0';
	}
	return ret;
}

// C23 compliant implementation of strndup
// Allocates a buffer of (strnlen(s, size) + 1) bytes.
TH_CALLER_FREE extern inline char* strndup(const char* src, size_t size) {
	return strdup_size(src, strnlen(src, size));
}

TH_NOINLINE int TH_VECTORCALL ascii_stricmp(const char* str1, const char* str2);
TH_NOINLINE int TH_VECTORCALL ascii_strnicmp(const char* str1, const char* str2, size_t count);

#ifdef __cplusplus
extern "C++" {

// Custom strdup variants that efficiently concatenate 2-3 strings.
// Structured specifically so that the compiler can optimize the copy
// operations into MOVs for any string literals.

TH_CALLER_FREE inline char* strdup_cat(std::string_view str1, std::string_view str2) {
	const size_t total_size = str1.length() + str2.length();
	char* ret = (char*)malloc(total_size + 1);
	ret[total_size] = '\0';
	char* ret_temp = ret;
	ret_temp += str1.copy(ret_temp, str1.length());
	str2.copy(ret_temp, str2.length());
	return ret;
}

TH_CALLER_FREE inline char* strdup_cat(std::string_view str1, char sep, std::string_view str2) {
	const size_t total_size = str1.length() + sizeof(sep) + str2.length();
	char* ret = (char*)malloc(total_size + 1);
	ret[total_size] = '\0';
	char* ret_temp = ret;
	ret_temp += str1.copy(ret_temp, str1.length());
	*ret_temp++ = sep;
	str2.copy(ret_temp, str2.length());
	return ret;
}

TH_CALLER_FREE inline char* strdup_cat(std::string_view str1, std::string_view str2, std::string_view str3) {
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

#if !TH_X64
#define _BitScanReverseZ(index, mask) _BitScanReverse((index), (mask))
#else
#define _BitScanReverseZ(index, mask) _BitScanReverse64((index), (mask))
#endif

#define is_valid_decimal(c) ((uint8_t)((c) - '0') < 10)

// Returns whether [c] is a valid hexadecimal character
TH_DEPRECATED_EXPORT bool (is_valid_hex)(char c);

inline bool is_valid_hex_inline(char c) {
	c |= 0x20;
	return is_valid_decimal(c) | ((uint8_t)(c - 'a') < 6);
}

#define is_valid_hex(c) is_valid_hex_inline(c)

// Efficiently tests if [value] is within the range [min, max)
inline bool int_in_range_exclusive(int32_t value, int32_t min, int32_t max) {
	return (uint32_t)(value - min) < (uint32_t)(max - min);
}
// Efficiently tests if [value] is within the range [min, max]
// Valid for both signed and unsigned integers
inline bool int_in_range_inclusive(int32_t value, int32_t min, int32_t max) {
	return (uint32_t)(value - min) <= (uint32_t)(max - min);
}

// Returns either the hexadecimal value of [c]
// or -1 if [c] is not a valid hexadecimal character
TH_DEPRECATED_EXPORT int8_t (hex_value)(char c);

inline int8_t hex_value_inline(char c) {
	c |= 0x20;
	c -= '0';
	if ((uint8_t)c < 10) return c;
	c -= 49;
	if ((uint8_t)c < 6) return c + 10;
	return -1;
}

#define hex_value(c) hex_value_inline(c)

#ifdef __cplusplus

extern "C++" {
	template <int size>
	using int_width_type = std::conditional_t<std::is_same_v<std::integral_constant<int, size>, std::integral_constant<int, sizeof(int8_t)>>, int8_t,
		std::conditional_t<std::is_same_v<std::integral_constant<int, size>, std::integral_constant<int, sizeof(int16_t)>>, int16_t,
		std::conditional_t<std::is_same_v<std::integral_constant<int, size>, std::integral_constant<int, sizeof(int32_t)>>, int32_t,
		std::conditional_t<std::is_same_v<std::integral_constant<int, size>, std::integral_constant<int, sizeof(int64_t)>>, int64_t,
		void>>>>;

	template <int size>
	using uint_width_type = std::conditional_t<std::is_same_v<std::integral_constant<int, size>, std::integral_constant<int, sizeof(uint8_t)>>, uint8_t,
		std::conditional_t<std::is_same_v<std::integral_constant<int, size>, std::integral_constant<int, sizeof(uint16_t)>>, uint16_t,
		std::conditional_t<std::is_same_v<std::integral_constant<int, size>, std::integral_constant<int, sizeof(uint32_t)>>, uint32_t,
		std::conditional_t<std::is_same_v<std::integral_constant<int, size>, std::integral_constant<int, sizeof(uint64_t)>>, uint64_t,
		void>>>>;
}

// Packs the bytes [c1], [c2], [c3], and [c4] together as a little endian integer
constexpr uint32_t TextInt(uint8_t c1, uint8_t c2 = 0, uint8_t c3 = 0, uint8_t c4 = 0) {
	return c4 << 24 | c3 << 16 | c2 << 8 | c1;
}
// Packs the bytes [c1], [c2], [c3], [c4], [c5], [c6], [c7], and [c8] together as a little endian integer
constexpr uint64_t TextInt64(uint8_t c1, uint8_t c2 = 0, uint8_t c3 = 0, uint8_t c4 = 0, uint8_t c5 = 0, uint8_t c6 = 0, uint8_t c7 = 0, uint8_t c8 = 0) {
	return (uint64_t)c8 << 56 | (uint64_t)c7 << 48 | (uint64_t)c6 << 40 | (uint64_t)c5 << 32 | c4 << 24 | c3 << 16 | c2 << 8 | c1;
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
