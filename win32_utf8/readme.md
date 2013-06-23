Win32 UTF-8 wrapper
-------------------

### Why a wrapper? ###

This library evolved from the need of the [Touhou Community Reliant Automatic Patcher] (https://github.com/nmlgc/thcrap) to hack Unicode functionality for the Win32 API into games using the ANSI functions.

By simply including ´win32_utf8.h´ and linking to this library, you automatically have Unicode compatibility in applications using the native Win32 APIs, usually without requiring changes to existing code using char strings.
