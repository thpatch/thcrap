Win32 UTF-8 wrapper
-------------------

### Why a wrapper? ###

This library evolved from the need of the [Touhou Community Reliant Automatic Patcher] (https://github.com/nmlgc/thcrap) to hack Unicode functionality for the Win32 API into games using the ANSI functions.

By simply including `win32_utf8.h` and linking to this library, you automatically have Unicode compatibility in applications using the native Win32 APIs, usually without requiring changes to existing code using char strings.

### Extended functionality ###

In addition, this library also adds new useful functionality to some original Windows functions.

###### kernel32.dll ######

* `CreateDirectoryU()` works recursively - the function creates all necessary directories to form the given path.
* `GetModuleFileNameU()` returns the necessary length of a buffer to hold the module file name if NULL is passed for `nSize` or `lpFilename`, similar to what `GetCurrentDirectory()` can do by default.

###### shlwapi.dll ######

* `PathRemoveFileSpecU()` correctly works as intended for paths containing forward slashes
