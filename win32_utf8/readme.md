Win32 UTF-8 wrapper
-------------------

### Pourquoi un wrapper? ###

Cette biliothèque a évolué selon les besoins de [Touhou Community Reliant Automatic Patcher] (https://github.com/nmlgc/thcrap) Pour hacker les fonctionalités Unicode pour les API Win32 dans les jeux utilisant des fonctions ANSI

En incluant `win32_utf8.h` et en le liant à cette bibliotheque, vous obtenez directement une compatibilité dans les appliquations utilisant les APIs natives de Win32, et cela normalement sans avoir à modifier le code existant qui utilise des chaines de caractères.
