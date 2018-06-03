// Copy-pasted from https://github.com/brliron/135tk/tree/master/bmpfont

#ifndef BMPFONT_CREATE_H_
# define BMPFONT_CREATE_H_

#include <jansson.h>

#ifdef __cplusplus
extern "C" {
#endif

// Main
BYTE *generate_bitmap_font(bool *chars_list, int chars_count, json_t *config, size_t *output_size);
		
// Called at the beginning of the program.
// The return value will be given to all the other functions in the obj parameter.
// NULL means the function failed. If you don't have any object to return, just return (void*)1 or font.
void* graphics_init(json_t *config);

// Called after all calls to the graphics_* functions. Use it to free the things allocated in graphics_init.
void graphics_free(void* obj);

// This function should draw the character c into dest.
// Dest is an array of 256x256 32bpp pixels, filled with black.
// After the call, w and h should contain the width and height of the character in dest.
void graphics_put_char(void* obj, WCHAR c, BYTE** dest, int* w, int* h);

#ifdef __cplusplus
}
#endif

#endif /* !BMPFONT_CREATE_H_ */
