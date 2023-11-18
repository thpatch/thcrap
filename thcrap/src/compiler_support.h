/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Support macros to make compiler-specific features
  * more portable.
  *
  * WARNING: Just because a compiler is listed here
  * does *NOT* mean that compiler is officially
  * supported for thcrap development!
  */

#pragma once

#include <stdint.h>

// Macros used for concatenating __COUNTER__ and __LINE__
// because of course it doesn't just work normally.
#define TH_MACRO_CONCAT_INNER(x, y) x ## y
#define TH_MACRO_CONCAT(x, y) TH_MACRO_CONCAT_INNER(x, y)

// - Compiler Detection
// Note: Intentionally tested for individually since
// some compilers like to define each others' detection
// macros to indicate compatibility.
#ifdef _MSC_VER
#define MSVC_COMPAT 1
#endif
#ifdef __GNUC__
#define GCC_COMPAT 1
#endif
#ifdef __clang__
#define CLANG_COMPAT 1
#endif
#ifdef __MINGW32__
#define MINGW_COMPAT 1
#endif
#ifdef __INTEL_COMPILER
#define ICC_OLD_COMPAT 1
#define ICC_COMPAT 1
#endif
#ifdef __INTEL_LLVM_COMPILER
#define ICC_NEW_COMPAT 1
#define ICC_COMPAT 1
#endif

// Apparently __has_include is part of the C++17 standard
// despite using the double underscore naming usually reserved
// for compiler internal use. Weird.
#ifndef __has_include
#define __has_include(header) 1 // Hopefully?
#endif

// - C Version Detection
// Note: MSVC only defines __STDC__ when language extensions
// are disabled.
#if defined(__STDC__) || MSVC_COMPAT
#define C89 1
#define C90 1
#endif
#ifdef __STDC_VERSION__
#if __STDC_VERSION__ >= 199409L
#define C95 1
#endif
#if __STDC_VERSION__ >= 199901L
#define C99 1
#endif
#if __STDC_VERSION__ >= 201112L
#define C11 1
#endif
#if __STDC_VERSION__ >= 201710L
#define C17 1
#endif
#if __STDC_VERSION__ > 201710L
#define C2X 1
#endif
#endif

// - C++ Version Detection
#ifdef __cplusplus
#if __cplusplus >= 199711L
#define CPP98
#endif
#if __cplusplus >= 201103L
#define CPP11 1
#endif
#if __cplusplus >= 201402L
#define CPP14 1
#endif
#if __cplusplus >= 201703L
#define CPP17 1
#endif
#if __cplusplus > 201703L && defined(_MSC_VER)
#define CPP17LATEST 1
#endif
#if __cplusplus >= 202002L
#define CPP20 1
#endif
#endif

// - Attribute Detection
// Intentionally doesn't use C/C++ version macros
// to take advantage of compilers that define attribute
// testing macros regardless of language version.
// 
// Even though it *looks* like it'd be very poor
// practice to define macros like these, this
// actually follows the ISO recommendations for
// attribute testing:
// https://isocpp.org/std/standing-documents/sd-6-sg10-feature-test-recommendations#testing-for-the-presence-of-an-attribute-__has_cpp_attribute
//
// Similar patterns have been applied to other
// macros in this header based on similar logic.

// - C++ Attributes
// Note: This macro was only "officially" added in C++20
// despite attributes being introduced in C++11, so tests
// using it also check for the equivalent C++ version.
//
// Note2: GCC defines __has_cpp_attribute
// when compiling as C, so exclude that
#if !defined(__cplusplus) || !defined(__has_cpp_attribute)
#ifdef __has_cpp_attribute
#undef __has_cpp_attribute
#endif
#define __has_cpp_attribute(attr) 0
#endif

// - C Attributes
// Note: C added both attributes and __has_c_attribute
// simultaneously in C2X, thus sparing it from the pains
// involved with __has_cpp_attribute.
#ifndef __has_c_attribute
#define __has_c_attribute(attr) 0
#endif

// - GNU Attributes
#ifndef __has_attribute
#define __has_attribute(attr) 0
#endif

// - Clang-specific Detection of MSVC Attributes
// Note: MSVC does not define this macro, so this
// abomination has to exist.
#ifndef __has_declspec_attribute
#if MSVC_COMPAT
#define TH_SAD_MSVC_DECLSPEC_TEST_align 1
#define TH_SAD_MSVC_DECLSPEC_TEST_allocate 1
#define TH_SAD_MSVC_DECLSPEC_TEST_allocator 1
#define TH_SAD_MSVC_DECLSPEC_TEST_appdomain 1
#define TH_SAD_MSVC_DECLSPEC_TEST_code_seg 1
#define TH_SAD_MSVC_DECLSPEC_TEST_deprecated 1
#define TH_SAD_MSVC_DECLSPEC_TEST_dllimport 1
#define TH_SAD_MSVC_DECLSPEC_TEST_dllexport 1
#define TH_SAD_MSVC_DECLSPEC_TEST_empty_bases 1
#define TH_SAD_MSVC_DECLSPEC_TEST_jitintrinsic 1
#define TH_SAD_MSVC_DECLSPEC_TEST_naked 1
#define TH_SAD_MSVC_DECLSPEC_TEST_noalias 1
#define TH_SAD_MSVC_DECLSPEC_TEST_noinline 1
#define TH_SAD_MSVC_DECLSPEC_TEST_noreturn 1
#define TH_SAD_MSVC_DECLSPEC_TEST_nothrow 1
#define TH_SAD_MSVC_DECLSPEC_TEST_novtable 1
#define TH_SAD_MSVC_DECLSPEC_TEST_no_sanitize_address 1
#define TH_SAD_MSVC_DECLSPEC_TEST_process 1
#define TH_SAD_MSVC_DECLSPEC_TEST_property 1
#define TH_SAD_MSVC_DECLSPEC_TEST_restrict 1
#define TH_SAD_MSVC_DECLSPEC_TEST_safebuffers 1
#define TH_SAD_MSVC_DECLSPEC_TEST_selectany 1
#define TH_SAD_MSVC_DECLSPEC_TEST_spectre 1
#define TH_SAD_MSVC_DECLSPEC_TEST_thread 1
#define TH_SAD_MSVC_DECLSPEC_TEST_uuid 1
#define __has_declspec_attribute(attr) TH_MACRO_CONCAT(TH_SAD_MSVC_DECLSPEC_TEST_, attr)
#else
#define __has_declspec_attribute(attr) 0
#endif
#endif

// - Builtin Intrinsics
#ifndef __has_builtin
#define __has_builtin(name) 0
#endif

// - Detection of __COUNTER__ macro
// Note: It seems as though testing whether __COUNTER__
// is defined ends up incrementing the value.
// 
// Note2: ICC documentation says that __COUNTER__ is
// "Defined as '0'.", which just makes everything harder.
// https://software.intel.com/content/www/us/en/develop/documentation/cpp-compiler-developer-guide-and-reference/top/compiler-reference/macros/additional-predefined-macros.html
#if GCC_COMPAT || CLANG_COMPAT || MSVC_COMPAT 
#define HAS_COUNTER_MACRO 1
#elif ICC_COMPAT
#define HAS_COUNTER_MACRO 0
#elif defined(__COUNTER__)
#define HAS_COUNTER_MACRO 1
#endif

// - Language/Version Independent Static Assert
// Note: Pre C/C++11 versions based on this:
// https://stackoverflow.com/questions/3385515/static-assert-in-c/3385694#3385694
#if CPP11
#define TH_STATIC_ASSERT(cond, message) static_assert(cond, message)
#elif C11
#define TH_STATIC_ASSERT(cond, message) _Static_assert(cond, message)
#elif HAS_COUNTER_MACRO
#define TH_STATIC_ASSERT(cond, message) typedef char TH_MACRO_CONCAT(static_assertion_, __COUNTER__)[(cond)?1:-1]
#else
#define TH_STATIC_ASSERT(cond, message) typedef char TH_MACRO_CONCAT(static_assertion_, __LINE__)[(cond)?1:-1]
#endif

// Utility macro to require otherwise "empty" statement
// macros to still end with a semicolon so that a syntax
// error won't be generated on compilers that support them.
#define TH_EMPTY_STATEMENT do; while(0)
// All of the above macro definitions for static assert
// are valid at file scope, so this can be used as a
// more general version of the classic "do; while(0)"
// macro for requiring a semicolon if it's ever needed.
#define TH_EMPTY_STATEMENT_FILE_SCOPE TH_STATIC_ASSERT(1,"")

// - [[nodiscard]] Equivalent Attributes
#if CPP20 || __has_cpp_attribute(nodiscard) >= 201907L
#define TH_NODISCARD [[nodicard]]
#define TH_NODISCARD_REASON(reason) [[nodiscard(reason)]]
#elif CPP17 || __has_cpp_attribute(nodiscard) >= 201603L
#define TH_NODISCARD [[nodicard]]
#define TH_NODISCARD_REASON(reason) [[nodiscard]]
#elif __has_c_attribute(nodiscard) >= 202003L
#define TH_NODISCARD [[nodicard]]
#define TH_NODISCARD_REASON(reason) [[nodiscard(reason)]]
#elif __has_attribute(warn_unused_result)
#define TH_NODISCARD __attribute__((warn_unused_result))
#define TH_NODISCARD_REASON(reason) __attribute__((warn_unused_result))
#else
#define TH_NODISCARD
#define TH_NODISCARD_REASON(reason)
#endif

// - [[deprecated]] Equivalent Attributes
#if CPP14 || __has_cpp_attribute(deprecated) >= 201309L
#define TH_DEPRECATED [[deprecated]]
#define TH_DEPRECATED_REASON(reason) [[deprecated(reason)]]
#elif __has_c_attribute(deprecated) >= 201904L
#define TH_DEPRECATED [[deprecated]]
#define TH_DEPRECATED_REASON(reason) [[deprecated(reason)]]
#elif __has_attribute(deprecated)
#define TH_DEPRECATED __attribute__((deprecated))
#define TH_DEPRECATED_REASON(reason) __attribute__((deprecated(reason)))
#elif __has_declspec_attribute(deprecated)
#define TH_DEPRECATED __declspec(deprecated)
#define TH_DEPRECATED_REASON(reason) __declspec(deprecated(reason))
#else
#define TH_DEPRECATED
#define TH_DEPRECATED_REASON(reason)
#endif

// - [[noreturn]] Equivalent Attributes
#if CPP11 || __has_cpp_attribute(noreturn) >= 200809L
#define TH_NORETURN [[noreturn]]
#elif C11
#define TH_NORETURN _Noreturn
#elif __has_attribute(noreturn)
#define TH_NORETURN __attribute((noreturn))
#elif __has_declspec_attribute(noreturn)
#define TH_NORETURN __declspec(noreturn)
#else
#define TH_NORETURN
#endif

// - [[fallthrough]] Equivalent Attributes
#if CPP17 || __has_cpp_attribute(fallthrough) >= 201603L
#define TH_FALLTHROUGH [[fallthrough]]
#elif __has_c_attribute(fallthrough) >= 201904L
#define TH_FALLTHROUGH [[fallthrough]]
#elif __has_attribute(fallthrough)
#define TH_FALLTHROUGH __attribute__((fallthrough))
#else
#define TH_FALLTHROUGH TH_EMPTY_STATEMENT
#endif

// - [[likely]] Equivalent Attributes
#if __has_cpp_attribute(likely) >= 201803L
#define TH_LIKELY [[likely]]
#elif __has_c_attribute(clang::likely)
#define TH_LIKELY [[clang::likely]]
#else
#define TH_LIKELY
#endif

// - [[unlikely]] Equivalent Attributes
#if __has_cpp_attribute(unlikely) >= 201803L
#define TH_UNLIKELY [[unlikely]]
#elif __has_c_attribute(clang::unlikely)
#define TH_UNLIKELY [[clang::unlikely]]
#else
#define TH_UNLIKELY
#endif

// - Unreachable Code Hint
// Note: MSVC __assume(0) is documented here:
// https://docs.microsoft.com/en-us/cpp/intrinsics/assume
#if __has_builtin(__builtin_unreachable)
#define TH_UNREACHABLE __builtin_unreachable()
#elif MSVC_COMPAT
#define TH_UNREACHABLE __assume(0)
#elif defined(__cpp_lib_unreachable) && __cpp_lib_unreachable >= 202202L
extern "C++" {
#include <utility>
}
#define TH_UNREACHABLE std::unreachable()
#elif C2X
#include <stddef.h>
#define TH_UNREACHABLE unreachable()
#elif defined(assert)
#define TH_UNREACHABLE assert(0)
#else
#define TH_UNREACHABLE TH_EMPTY_STATEMENT
#endif

// - Optimization Hint
// Note: GCC-compatible version from:
// https://stackoverflow.com/questions/25667901/assume-clause-in-gcc/26195434#26195434
#if __has_builtin(__builtin_assume) // Clang
#define TH_ASSUME(cond) __builtin_assume(cond)
#elif __has_attribute(assume) // GCC
#define TH_ASSUME(cond) __attibute__((assume(cond)))
#elif MSVC_COMPAT
#define TH_ASSUME(cond) __assume(cond)
#elif __has_cpp_attribute(assume)
#define TH_ASSUME(cond) [[assume((cond))]]
#elif __has_builtin(__builtin_unreachable)
#define TH_ASSUME(cond) do { if (!(cond)) __builtin_unreachable(); } while (0)
#else
#define TH_ASSUME(cond) TH_EMPTY_STATEMENT
#endif

#if defined(NDEBUG) && !defined(TH_DISABLE_OPT_ASSERT)
// Variant of assert that uses the condition as
// a normal assert in debug mode and as a hint
// to the compiler in release mode. This hint is
// a promise that the condition will *always* be
// false. Undefined behavior occurs if the
// condition is true.
#define TH_OPTIMIZING_ASSERT(cond) TH_ASSUME(cond)
#elif defined(assert)
// Variant of assert that uses the condition as
// a normal assert in debug mode and as a hint
// to the compiler in release mode. This hint is
// a promise that the condition will *always* be
// false. Undefined behavior occurs if the
// condition is true.
#define TH_OPTIMIZING_ASSERT(cond) assert(cond)
#else
#define TH_OPTIMIZING_ASSERT(cond) TH_EMPTY_STATEMENT
#endif

// - Eval Arguments for Syntax Errors
// Note: MSVC "converts to 0 at compile time", thus
// the weird definition for non-MSVC.
#if MSVC_COMPAT
#define TH_EVAL_NOOP(...) __noop(__VA_ARGS__)
#else
#define TH_EVAL_NOOP(...) (((void)sizeof printf("", __VA_ARGS__)), 0)
#endif

// - C99 restrict qualifier
#if C99
#define TH_RESTRICT restrict
#elif MSVC_COMPAT
#define TH_RESTRICT __restrict
#else
#define TH_RESTRICT
#endif

// - DLL Export Attribute
// Todo: Are the C++20 export/import keywords related to any of this?
#if __has_declspec_attribute(dllexport)
#define TH_EXPORT __declspec(dllexport)
#elif __has_attribute(dllexport)
#define TH_EXPORT __attribute__((dllexport))
#else
#define TH_EXPORT
#endif

// - DLL Import Attribute
#if __has_declspec_attribute(dllimport)
#define TH_IMPORT __declspec(dllimport)
#elif __has_attribute(dllimport)
#define TH_IMPORT __attribute__((dllimport))
#else
#define TH_IMPORT
#endif

// - GCC-specific MSVC-Compatibility Attribute
#if __has_attribute(ms_abi)
#define TH_MS_ABI __attribute__((ms_abi))
#else
#define TH_MS_ABI
#endif

// windows.h defines cdecl as an empty macro
#ifdef _WINDOWS_
#ifdef cdecl
#define WORK_AROUND_CDECL_JANK
#pragma push_macro("cdecl")
#undef cdecl
#endif
#endif

// - cdecl Calling Convention
// -Arguments are passed right-to-left on the stack.
// -Caller cleans the stack.
#if __has_attribute(cdecl)
#define TH_CDECL __attribute__((cdecl))
#elif MSVC_COMPAT || CLANG_COMPAT || ICC_COMPAT
#define TH_CDECL __cdecl
#else
#define TH_CDECL
#endif

#ifdef WORK_AROUND_CDECL_JANK
#undef WORK_AROUND_CDECL_JANK
#pragma pop_macro("cdecl")
#endif

// - stdcall Calling Convention
// -Arguments are passed right-to-left on the stack.
// -Callee cleans the stack.
#if __has_attribute(stdcall)
#define TH_STDCALL __attribute__((stdcall))
#elif MSVC_COMPAT || CLANG_COMPAT || ICC_COMPAT
#define TH_STDCALL __stdcall
#else
#define TH_STDCALL
#endif

// - fastcall Calling Convention
// -First two arguments are passed in ECX and EDX
// if possible and remaining arguments are passed
// right-to-left on the stack.
// -Callee cleans the stack.
#if __has_attribute(fastcall)
#define TH_FASTCALL __attribute__((fastcall))
#elif MSVC_COMPAT || CLANG_COMPAT || ICC_COMPAT
#define TH_FASTCALL __fastcall
#else
#define TH_FASTCALL
#endif

// - regcall Calling Convention
// -As many arguments as possible are passed in
// registers.
#if __has_attribute(regcall)
#define TH_REGCALL __attribute__((regcall))
#elif CLANG_COMPAT || ICC_COMPAT
#define TH_REGCALL __regcall
#else
#define TH_REGCALL
#endif

// - regparm Calling Convention
// -First three arguments up to (num) are passed
// in EAX, EDX, and ECX if possible and remaining
// arguments are passed right-to-left on the stack.
#if __has_attribute(regparm)
#define TH_REGPARM(num) __attribute__((regparm(num)))
#elif CLANG_COMPAT
#define TH_REGPARM(num) __attribute__((regparm(num)))
#else
#define TH_REGPARM(num)
#endif

// - thiscall Calling Convention
#if __has_attribute(thiscall)
#define TH_THISCALL __attribute__((thiscall))
#elif MSVC_COMPAT || CLANG_COMPAT || ICC_COMPAT
#define TH_THISCALL __thiscall
#else
#define TH_THISCALL
#endif

// - vectorcall Calling Convention
#if __has_attribute(vectorcall)
#define TH_VECTORCALL __attribute__((vectorcall))
#elif __has_attribute(sseregparm)
#define TH_VECTORCALL __attribute__((sseregparm))
#elif MSVC_COMPAT || CLANG_COMPAT || ICC_COMPAT
#define TH_VECTORCALL __vectorcall
#else
#define TH_VECTORCALL
#endif

// - Cold Function Attribute
// Note: Only functional on GCC but still useful
// for documentation purposes.
#if __has_attribute(cold)
#define TH_COLD __attribute__((cold))
#else
#define TH_COLD
#endif

// - Hot Function Attribute
// Note: Only functional on GCC but still useful
// for documentation purposes.
#if __has_attribute(hot)
#define TH_HOT __attribute__((hot))
#else
#define TH_HOT
#endif

// - NoInline Equivalent Attribute
#if __has_attribute(noinline)
#define TH_NOINLINE __attribute__((noinline))
#elif __has_declspec_attribute(noinline)
#define TH_NOINLINE __declspec(noinline)
#else
#define TH_NOINLINE
#endif

// - ForceInline Equivalent Attribute
#if __has_attribute(always_inline)
#define TH_FORCEINLINE __attribute__((always_inline))
#elif MSVC_COMPAT
#define TH_FORCEINLINE __forceinline
#else
#define TH_FORCEINLINE inline
#endif

// - Naked Equivalent Attribute
#if __has_attribute(naked)
#define TH_NAKED __attribute__((naked))
#elif __has_declspec_attribute(naked)
#define TH_NAKED __declspec(naked)
#else
#define TH_NAKED
#endif

// - Segmented Addressing
#if __has_attribute(address_space)
#define read_fs_byte(offset) (*(uint8_t __attribute__((address_space(257)))*)((uintptr_t)offset))
#define read_fs_word(offset) (*(uint16_t __attribute__((address_space(257)))*)((uintptr_t)offset))
#define read_fs_dword(offset) (*(uint32_t __attribute__((address_space(257)))*)((uintptr_t)offset))
#define write_fs_byte(offset, data) ((void)(*(uint8_t __attribute__((address_space(257)))*)((uintptr_t)offset) = (uint8_t)(data)))
#define write_fs_word(offset, data) ((void)(*(uint16_t __attribute__((address_space(257)))*)((uintptr_t)offset) = (uint16_t)(data)))
#define write_fs_dword(offset, data) ((void)(*(uint32_t __attribute__((address_space(257)))*)((uintptr_t)offset) = (uint32_t)(data)))
#elif defined(__SEG_FS) && (CLANG_COMPAT || !defined(__cplusplus)) // __seg_fs isn't recognized by GCC when compiling C++
#define read_fs_byte(offset) (*(__seg_fs uint8_t*)((uintptr_t)offset))
#define read_fs_word(offset) (*(__seg_fs uint16_t*)((uintptr_t)offset))
#define read_fs_dword(offset) (*(__seg_fs uint32_t*)((uintptr_t)offset))
#define write_fs_byte(offset, data) ((void)(*(__seg_fs uint8_t*)((uintptr_t)offset) = (uint8_t)(data)))
#define write_fs_word(offset, data) ((void)(*(__seg_fs uint16_t*)((uintptr_t)offset) = (uint16_t)(data)))
#define write_fs_dword(offset, data) ((void)(*(__seg_fs uint32_t*)((uintptr_t)offset) = (uint32_t)(data)))
#elif MSVC_COMPAT
#define read_fs_byte(offset) __readfsbyte(offset)
#define read_fs_word(offset) __readfsword(offset)
#define read_fs_dword(offset) __readfsdword(offset)
#define write_fs_byte(offset, data) __writefsbyte(offset, data)
#define write_fs_word(offset, data) __writefsword(offset, data)
#define write_fs_dword(offset, data) __writefsdword(offset, data)
#else
#define read_fs_byte(offset) (*(uint8_t*)((uintptr_t)offset))
#define read_fs_word(offset) (*(uint16_t*)((uintptr_t)offset))
#define read_fs_dword(offset) (*(uint32_t*)((uintptr_t)offset))
#define write_fs_byte(offset, data) ((void)(*(uint8_t*)((uintptr_t)offset) = (uint8_t)(data)))
#define write_fs_word(offset, data) ((void)(*(uint16_t*)((uintptr_t)offset) = (uint16_t)(data)))
#define write_fs_dword(offset, data) ((void)(*(uint32_t*)((uintptr_t)offset) = (uint32_t)(data)))
#endif
#if __has_attribute(address_space)
#define read_gs_byte(offset) (*(uint8_t __attribute__((address_space(256)))*)((uintptr_t)offset))
#define read_gs_word(offset) (*(uint16_t __attribute__((address_space(256)))*)((uintptr_t)offset))
#define read_gs_dword(offset) (*(uint32_t __attribute__((address_space(256)))*)((uintptr_t)offset))
#define read_gs_qword(offset) (*(uint64_t __attribute__((address_space(256)))*)((uintptr_t)offset))
#define write_gs_byte(offset, data) ((void)(*(uint8_t __attribute__((address_space(256)))*)((uintptr_t)offset) = (uint8_t)(data)))
#define write_gs_word(offset, data) ((void)(*(uint16_t __attribute__((address_space(256)))*)((uintptr_t)offset) = (uint16_t)(data)))
#define write_gs_dword(offset, data) ((void)(*(uint32_t __attribute__((address_space(256)))*)((uintptr_t)offset) = (uint32_t)(data)))
#define write_gs_qword(offset, data) ((void)(*(uint64_t __attribute__((address_space(256)))*)((uintptr_t)offset) = (uint64_t)(data)))
#elif defined(__SEG_GS) && (CLANG_COMPAT || !defined(__cplusplus)) // __seg_gs isn't recognized by GCC when compiling C++
#define read_gs_byte(offset) (*(__seg_gs uint8_t*)((uintptr_t)offset))
#define read_gs_word(offset) (*(__seg_gs uint16_t*)((uintptr_t)offset))
#define read_gs_dword(offset) (*(__seg_gs uint32_t*)((uintptr_t)offset))
#define read_gs_qword(offset) (*(__seg_gs uint64_t*)((uintptr_t)offset))
#define write_gs_byte(offset, data) ((void)(*(__seg_gs uint8_t*)((uintptr_t)offset) = (uint8_t)(data)))
#define write_gs_word(offset, data) ((void)(*(__seg_gs uint16_t*)((uintptr_t)offset) = (uint16_t)(data)))
#define write_gs_dword(offset, data) ((void)(*(__seg_gs uint32_t*)((uintptr_t)offset) = (uint32_t)(data)))
#define write_gs_qword(offset, data) ((void)(*(__seg_gs uint64_t*)((uintptr_t)offset) = (uint64_t)(data)))
#elif MSVC_COMPAT
#define read_gs_byte(offset) __readgsbyte(offset)
#define read_gs_word(offset) __readgsword(offset)
#define read_gs_dword(offset) __readgsdword(offset)
#define read_gs_qword(offset) __readgsqword(offset)
#define write_gs_byte(offset, data) __writegsbyte(offset, data)
#define write_gs_word(offset, data) __writegsword(offset, data)
#define write_gs_dword(offset, data) __writegsdword(offset, data)
#define write_gs_qword(offset, data) __writegsqword(offset, data)
#else
#define read_gs_byte(offset) (*(uint8_t*)((uintptr_t)offset))
#define read_gs_word(offset) (*(uint16_t*)((uintptr_t)offset))
#define read_gs_dword(offset) (*(uint32_t*)((uintptr_t)offset))
#define read_gs_qword(offset) (*(uint64_t*)((uintptr_t)offset))
#define write_gs_byte(offset, data) ((void)(*(uint8_t*)((uintptr_t)offset) = (uint8_t)(data)))
#define write_gs_word(offset, data) ((void)(*(uint16_t*)((uintptr_t)offset) = (uint16_t)(data)))
#define write_gs_dword(offset, data) ((void)(*(uint32_t*)((uintptr_t)offset) = (uint32_t)(data)))
#define write_gs_qword(offset, data) ((void)(*(uint64_t*)((uintptr_t)offset) = (uint64_t)(data)))
#endif
