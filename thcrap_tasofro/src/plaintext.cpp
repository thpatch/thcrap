/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Patching of plaintext files.
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"

int patch_plaintext(void *file_inout, size_t size_out, size_t size_in, json_t *patch)
{
	char* file_out = (char*)file_inout;
	char* file_in;
	char* buffer_in;
	size_t line;

	// Make a copy of the input buffer
	buffer_in = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size_in);
	memcpy(buffer_in, file_inout, size_in);
	file_in = buffer_in;

	line = 1;
	while (size_in > 0) {
		char* end_line = file_in;
		while (size_in > 0 && *end_line != '\n') {
			end_line++;
			size_in--;
		}
		if (size_in > 0) {
			end_line++;
			size_in--;
		}

		json_t *lines = json_object_numkey_get(patch, line);
		if (json_flex_array_size(lines) > 0) {
			*file_out = '"';
			file_out++;

			size_t ind;
			json_t *val;
			json_flex_array_foreach(lines, ind, val) {
				if (ind > 0) {
					memcpy(file_out, "\\n", 2);
					file_out += 2;
				}
				const char* str = json_string_value(val);
				for (size_t i = 0; i < strlen(str); i++) {
					if (str[i] == '"') {
						if (ind == 0 && i == 0) {
							*file_out = ' ';
							file_out++;
						}
						*file_out = '"';
						file_out++;
					}
					*file_out = str[i];
					file_out++;
				}
			}
			memcpy(file_out, "\"\r\n", 3);
			file_out += 3;
		}
		else {
			memcpy(file_out, file_in, end_line - file_in);
			file_out += end_line - file_in;
		}
		file_in = end_line;
		line++;
	}

	HeapFree(GetProcessHeap(), 0, buffer_in);

	return 0;
}
