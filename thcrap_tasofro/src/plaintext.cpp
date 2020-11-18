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

int patch_plaintext(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
	char* file_in = (char*)file_inout;
	std::string file_out;
	file_out.reserve(size_out);

	size_t line = 1;
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
			file_out += '"';

			size_t ind;
			json_t *val;
			json_flex_array_foreach(lines, ind, val) {
				if (ind > 0) {
					file_out += "\\n";
				}
				const char* str = json_string_value(val);
				for (size_t i = 0; i < strlen(str); i++) {
					if (str[i] == '"') {
						if (ind == 0 && i == 0) {
							file_out += ' ';
						}
						file_out += '"';
					}
					file_out += str[i];
				}
			}
			file_out += "\"\r\n";
		}
		else {
			file_out.append(file_in, end_line - file_in);
		}
		file_in = end_line;
		line++;
	}

	if (file_out.size() > size_out) {
		log_printf("%s: patched file doesn't fit inside the rep buffer!\n", fn);
		return 0;
	}
	memcpy(file_inout, file_out.c_str(), file_out.size());
	return 1;
}
