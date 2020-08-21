#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "strings_array.h"

char **strings_array_create()
{
    char **array = new char*[1];
    array[0] = nullptr;
    return array;
}

char **strings_array_add(char **old_array, const char *str)
{
    size_t size = strings_array_size(old_array);
    char **new_array = new char*[size + 2];

    memcpy(new_array, old_array, size * sizeof(char*));
    new_array[size] = strdup(str);
    new_array[size + 1] = nullptr;

    delete[] old_array;
    return new_array;
}

size_t strings_array_size(char **strings_array)
{
    size_t i;
    for (i = 0; strings_array[i]; i++) {
    }
    return i;
}

char **strings_array_create_and_fill(size_t nb_elems, ...)
{
    va_list args;
    va_start(args, nb_elems);

    char **array = new char*[nb_elems + 1];
    for (size_t i = 0; i < nb_elems; i++) {
        array[i] = strdup(va_arg(args, const char*));
    }
    array[nb_elems] = nullptr;

    va_end(args);
    return array;
}

void strings_array_free(char **strings_array)
{
    for (size_t i = 0; strings_array[i]; i++) {
        free(strings_array[i]);
    }
    delete[] strings_array;
}
