#include "thcrap.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "strings_array.h"

char **strings_array_create()
{
    char **array = (char**)malloc(sizeof(char*));
    array[0] = NULL;
    return array;
}

char **strings_array_add(char **array, const char *str)
{
    size_t size = strings_array_size(array);
    if (void* temp = realloc(array, (size + 2) * sizeof(char*))) {
        array = (char**)temp;
        array[size] = strdup(str);
        array[size + 1] = NULL;
    }
    return array;
}

size_t strings_array_size(char **strings_array)
{
    size_t i = 0;
    while (strings_array[i]) ++i;
    return i;
}

char **strings_array_create_and_fill(size_t nb_elems, ...)
{
    va_list args;
    va_start(args, nb_elems);

    char** array = (char**)malloc((nb_elems + 1) * sizeof(char*));
    array[nb_elems] = NULL;
    for (size_t i = 0; i < nb_elems; i++) {
        char* str = va_arg(args, char*);
        array[i] = str ? strdup(str) : str;
    }

    va_end(args);
    return array;
}

void strings_array_free(char **strings_array)
{
    for (size_t i = 0; strings_array[i]; i++) {
        free(strings_array[i]);
    }
    free(strings_array);
}
