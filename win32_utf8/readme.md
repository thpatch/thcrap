Win32 UTF-8 wrapper
-------------------

### Pourquoi un wrapper? ###

Cette biliothèque a évolué selon les besoins de [Touhou Community Reliant Automatic Patcher] (https://github.com/nmlgc/thcrap) Pour hacker les fonctionalités Unicode pour les API Win32 dans les jeux utilisant des fonctions ANSI

En incluant `win32_utf8.h` et en le liant à cette bibliotheque, vous obtenez directement une compatibilité dans les appliquations utilisant les APIs natives de Win32, et cela normalement sans avoir à modifier le code existant qui utilise des chaines de caractères.

### Extended functionality ###

In addition, this library also adds new useful functionality to some original Windows functions.

###### kernel32.dll ######

* `CreateDirectoryU()` works recursively - the function creates all necessary directories to form the given path.
* `GetModuleFileNameU()` returns the necessary length of a buffer to hold the module file name if NULL is passed for `nSize` or `lpFilename`, similar to what `GetCurrentDirectory()` can do by default.

###### shlwapi.dll ######

* `PathRemoveFileSpecU()` correctly works as intended for paths containing forward slashes
