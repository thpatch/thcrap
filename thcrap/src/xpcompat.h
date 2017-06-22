/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Abstractions and approximations of Win32 API features that are not
  * available on Windows XP.
  */

#pragma once

/// Slim Reader/Writer Locks
/// ------------------------
typedef void __stdcall srwlock_func_t(PSRWLOCK SRWLock);

#define AcquireSRWLockExclusive srwlock_funcs[0]
#define ReleaseSRWLockExclusive srwlock_funcs[1]
#define AcquireSRWLockShared srwlock_funcs[2]
#define ReleaseSRWLockShared srwlock_funcs[3]

THCRAP_API extern srwlock_func_t *srwlock_funcs[4];
/// ------------------------

#ifdef __cplusplus
/// Thread-local structures
/// -----------------------
// On Windows XP, implicit TLS using __declspec(thread) does not work for
// variables declared in DLLs that are loaded into the process after
// it started, using LoadLibrary(). (See http://www.nynaeve.net/?p=187.)
// (MinGW/GCC seems to support C11 _Thread_local, which would have saved us
// all of this.)
// The THREAD_LOCAL macro declares:
// • a TLS slot with the given [name]
// • a type-safe accessor function named [name]_get(void), returning a pointer
//   of the given [type] to the thread's instance of the structure. This
//   function can then be optionally exported to make the thread-local
//   structure available to other modules.
// • a _mod_thread_exit(void) function freeing a thread's instance of the
//   structure once it exits.
//
// [ctor] and [dtor] are optional constructor and destructor functions that
// are called for new instances. If given, the function signatures should
// match the tlsstruct_ctor_t and tlsstruct_dtor_t types declared below.
// (Since users will want to declare those using their own structure type, we
// can't type-check it at compile time, since we can't know that type, and we
// kinda want to keep the macro part of this as small as possible.)
// Passing a nullptr for these falls back on the default behavior: zeroing the
// memory on creation, and doing nothing on destruction.
#define THREAD_LOCAL(type, name, ctor, dtor) \
	TLSSlot name; \
	\
	type* name##_get(void) \
	{ \
		return (type *)tlsstruct_get(name.slot, sizeof(type), (tlsstruct_ctor_t *)ctor); \
	} \
	\
	extern "C" __declspec(dllexport) void name##_mod_thread_exit(void) \
	{ \
		tlsstruct_free(name.slot, (tlsstruct_dtor_t *)dtor); \
	}

typedef void tlsstruct_ctor_t(void *instance, size_t struct_size);
typedef void tlsstruct_dtor_t(void *instance);

// Internal functions
THCRAP_API void* tlsstruct_get(DWORD slot, size_t struct_size, tlsstruct_ctor_t *ctor);
THCRAP_API void tlsstruct_free(DWORD slot, tlsstruct_dtor_t *dtor);

struct TLSSlot {
	DWORD slot = TlsAlloc();

	~TLSSlot() { TlsFree(slot); }
};
/// -----------------------
#endif
