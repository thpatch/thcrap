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
#define unexpected(condition) (!(condition)) TH_LIKELY; TH_UNLIKELY else

/// Internal Windows Structs
/// TODO: Move these to some sort of header like windows_support.h
/// -------

/*
Struct definitions based on the fields documented to have
consistent offsets in all Windoes versions 5.0+
TEB: https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/api/pebteb/teb/index.htm
PEB: https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/api/pebteb/peb/index.htm
*/

typedef struct _PEB PEB;
struct _PEB {
	BOOLEAN InheritedAddressSpace;
	BOOLEAN ReadImageFileExecOptions;
	BOOLEAN BeingDebugged;
	BOOLEAN SpareBool;
#ifdef TH_X64
	UCHAR Padding0[4];
#endif
	HANDLE Mutant;
	PVOID ImageBaseAddress;
	PVOID Ldr; // PEB_LDR_DATA*
	PVOID ProcessParameters; // RTL_USER_PROCESS_PARAMETERS*
	PVOID SubSystemData;
	HANDLE ProcessHeap;
	RTL_CRITICAL_SECTION* FastPebLock;
	PVOID unreliable_member_1;
	PVOID unreliable_member_2;
	ULONG unreliable_member_3;
#ifdef TH_X64
	UCHAR Padding1[4];
#endif
	PVOID KernelCallbackTable;
	ULONG SystemReserved[2];
	PVOID unreliable_member_4;
	ULONG TlsExpansionCounter;
#ifdef TH_X64
	UCHAR Padding2[4];
#endif
	PVOID TlsBitmap;
	ULONG TlsBitmapBits[2];
	PVOID ReadOnlySharedMemoryBase;
	PVOID unreliable_member_5;
	PVOID* ReadOnlyStaticServerData;
	PVOID AnsiCodePageData;
	PVOID OemCodePageData;
	PVOID UnicodeCaseTableData;
	ULONG NumberOfProcessors;
	ULONG NtGlobalFlag;
	LARGE_INTEGER CriticalSectionTimeout;
	ULONG_PTR HeapSegmentReserve;
	ULONG_PTR HeapSegmentCommit;
	ULONG_PTR HeapDeCommitTotalFreeThreshold;
	ULONG_PTR HeapDeCommitFreeBlockThreshold;
	ULONG NumberOfHeaps;
	ULONG MaximumNumberOfHeaps;
	PVOID* ProcessHeaps;
	PVOID GdiSharedHandleTable;
	PVOID ProcessStarterHelper;
	ULONG GdiDCAttributeList;
#ifdef TH_X64
	UCHAR Padding3[4];
#endif
	RTL_CRITICAL_SECTION* LoaderLock;
	ULONG OSMajorVersion;
	ULONG OSMinorVersion;
	USHORT OSBuildNumber;
	union {
		USHORT OSCSDVersion;
		struct {
			BYTE OSCSDMajorVersion;
			BYTE OSCSDMinorVersion;
		};
	};
	ULONG OSPlatformId;
	ULONG ImageSubsystem;
	ULONG ImageSubsystemMajorVersion;
	ULONG ImageSubsystemMinorVersion;
#ifdef TH_X64
	UCHAR Padding4[4];
#endif
	KAFFINITY unreliable_member_6;
#ifdef TH_X64
	ULONG GdiHandleBuffer[0x3C];
#else
	ULONG GdiHandleBuffer[0x22];
#endif
	VOID(*PostProcessInitRoutine)(VOID);
	PVOID TlsExpansionBitmap;
	ULONG TlsExpansionBitmapBits[0x20];
	ULONG SessionId;
#ifdef TH_X64
	UCHAR Padding5[4];
#endif
};
typedef struct _CLIENT_ID {
	HANDLE UniqueProcess;
	HANDLE UniqueThread;
} CLIENT_ID;
typedef struct _GDI_TEB_BATCH {
	ULONG Offset;
	ULONG_PTR HDC;
	ULONG Buffer[310];
} GDI_TEB_BATCH, *PGDI_TEB_BATCH;
typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
typedef struct _TEB TEB;
struct _TEB {
	//NT_TIB NtTib;
	struct _EXCEPTION_REGISTRATION_RECORD* ExceptionList;
	PVOID StackBase;
	PVOID StackLimit;
	PVOID SubSystemTib;
	PVOID FiberData;
	PVOID ArbitraryUserPointer;
	TEB* Self;
	PVOID EnvironmentPointer;
	CLIENT_ID ClientId;
	PVOID ActiveRpcHandle;
	PVOID ThreadLocalStoragePointer;
	PEB* ProcessEnvironmentBlock;
	ULONG LastErrorValue;
	ULONG CountOfOwnedCriticalSections;
	PVOID CsrClientThread;
	PVOID Win32ThreadInfo;
	ULONG User32Reserved[0x1A];
	ULONG UserReserved[5];
	PVOID WOW32Reserved;
	ULONG CurrentLocale;
	ULONG FpSoftwareStatusRegister;
	PVOID SystemReserved1[0x36];
	LONG ExceptionCode;
#ifdef TH_X64
	UCHAR Padding0[4];
#endif
	UCHAR SpareBytes1[0x2C];
	GDI_TEB_BATCH GdiTebBatch;
	CLIENT_ID RealClientId;
	PVOID GdiCachedProcessHandle;
	ULONG GdiClientPID;
	ULONG GdiClientTID;
	PVOID GdiThreadLocalInfo;
	ULONG_PTR Win32ClientInfo[0x3E];
	PVOID glDispatchTable[0xE9];
	ULONG_PTR glReserved1[0x1D];
	PVOID glReserved2;
	PVOID glSectionInfo;
	PVOID glSection;
	PVOID glTable;
	PVOID glCurrentRC;
	PVOID glContext;
	ULONG LastStatusValue;
#ifdef TH_X64
	UCHAR Padding2[4];
#endif
	UNICODE_STRING StaticUnicodeString;
	union {
		WCHAR StaticUnicodeBuffer[MAX_PATH + 1];
		char StaticUTF8Buffer[(MAX_PATH + 1) * sizeof(WCHAR)];
	};
#ifdef TH_X64
	UCHAR Padding3[6];
#endif
	PVOID DeallocationStack;
	PVOID TlsSlots[0x40];
	LIST_ENTRY TlsLinks;
	PVOID Vdm;
	PVOID ReservedForNtRpc;
	HANDLE DbgSsReserved[2];
};

#ifdef TH_X64
#define read_teb_member(member) (\
member_size(TEB, member) == 1 ? read_gs_byte(offsetof(TEB, member)) : \
member_size(TEB, member) == 2 ? read_gs_word(offsetof(TEB, member)) : \
member_size(TEB, member) == 4 ? read_gs_dword(offsetof(TEB, member)) : \
read_gs_qword(offsetof(TEB, member)) \
)
#define write_teb_member(member, data) (\
member_size(TEB, member) == 1 ? write_gs_byte(offsetof(TEB, member), (data)) : \
member_size(TEB, member) == 2 ? write_gs_word(offsetof(TEB, member), (data)) : \
member_size(TEB, member) == 4 ? write_gs_dword(offsetof(TEB, member), (data)) : \
write_gs_qword(offsetof(TEB, member), (data)) \
)
#else
#define read_teb_member(member) (\
member_size(TEB, member) == 1 ? read_fs_byte(offsetof(TEB, member)) : \
member_size(TEB, member) == 2 ? read_fs_word(offsetof(TEB, member)) : \
read_fs_dword(offsetof(TEB, member)) \
)
#define write_teb_member(member, data) (\
member_size(TEB, member) == 1 ? write_fs_byte(offsetof(TEB, member), (data)) : \
member_size(TEB, member) == 2 ? write_fs_word(offsetof(TEB, member), (data)) : \
write_fs_dword(offsetof(TEB, member), (data)) \
)
#endif
#define CurrentTeb() ((TEB*)read_teb_member(Self))
#define CurrentPeb() ((PEB*)read_teb_member(ProcessEnvironmentBlock))

#define KernelSharedDataAddr (0x7FFE0000u)

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

#include <intrin.h>

THCRAP_API TH_CALLER_FREE inline char16_t* utf8_to_utf16(const char* utf8_str) {
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
	// strncpy will 0 pad
	if (ret && !memccpy(ret, src, '\0', size)) {
		ret[size] = '\0';
	}
	return ret;
}

// C23 compliant implementation of strndup
// Allocates a buffer of (strnlen(s, size) + 1) bytes.
inline TH_CALLER_FREE char* strndup(const char* src, size_t size) {
	return strdup_size(src, strnlen(src, size));
}

TH_NOINLINE int TH_VECTORCALL ascii_stricmp(const char* str1, const char* str2);
TH_NOINLINE int TH_VECTORCALL ascii_strnicmp(const char* str1, const char* str2, size_t count);

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

// Returns whether [c] is a valid hexadecimal character
bool is_valid_hex(char c);

#define is_valid_decimal(c) ((uint8_t)((c) - '0') < 10)

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
int8_t hex_value(char c);

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
