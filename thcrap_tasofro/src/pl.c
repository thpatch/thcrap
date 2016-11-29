/**
* Touhou Community Reliant Automatic Patcher
* Tasogare Frontier support plugin
*
* ----
*
* On-the-fly th145 pl patcher
*/

#include <thcrap.h>
#include "thcrap_tasofro.h"

#define MOVE_BUFF(buff, size, n) (buff) += n; (size) -= n;
#define PUT_CHAR(buff, size, c) *(buff) = c; MOVE_BUFF(buff, size, 1);

static void next_line(BYTE **file, size_t *size)
{
	while (*size > 0 && **file != '\n') {
		MOVE_BUFF(*file, *size, 1);
	}
	if (*size > 0) {
		MOVE_BUFF(*file, *size, 1);
	}
}

static BOOL ignore_line(BYTE *file, size_t size)
{
	while (size > 0 && *file == ' ') {
		MOVE_BUFF(file, size, 1);
	}
	if (size == 0) {
		return TRUE;
	}
	if (*file == '#' ||
		*file == ':' ||
		*file == ',' ||
		*file == '\r' ||
		*file == '\n') {
		return TRUE;
	}
	return FALSE;
}

static void copy_line(BYTE *file_in, size_t size_in, BYTE *file_out, size_t size_out)
{
	size_t i;

	for (i = 0; i < size_in && i < size_out && file_in[i] != '\n'; i++) {
		file_out[i] = file_in[i];
	}
	if (i < size_out) {
		file_out[i] = '\n';
	}
}

static void prepare_balloon(BYTE* balloon, int n_line, int total_line)
{
	// TODO: actually look for a nice balloon size
	strcpy(balloon + 5, "_1_1");
	balloon[6] = n_line + '0';
	balloon[8] = total_line + '0';
}

static void replace_line(BYTE *file_in, size_t size_in, BYTE **file_out, size_t *size_out, unsigned int balloon_nb, json_t *lines)
{
	BOOL line_break;
	BYTE balloon[10];
	unsigned int cur_line;
	unsigned int i;
	unsigned int nb_lines;

	// Skip the original text
	if (*file_in != '"') {
		while (size_in > 0 && *file_in != '\n' && *file_in != ',') {
			MOVE_BUFF(file_in, size_in, 1);
		}
		line_break = file_in[-1] == '\\';
	}
	else {
		MOVE_BUFF(file_in, size_in, 1);
		while (size_in > 0 && *file_in != '\n' && *file_in != '"') {
			MOVE_BUFF(file_in, size_in, 1);
		}
		line_break = file_in[-1] == '\\';
		MOVE_BUFF(file_in, size_in, 1);
	}
	MOVE_BUFF(file_in, size_in, 1);

	// Copy the balloon name from the original file
	for (i = 0; size_in > 0 && *file_in != '\n' && *file_in != ',' && i < 10; i++, file_in++, size_in--) {
		balloon[i] = *file_in;
	}

	// Write to the output file
	nb_lines = json_array_size(lines);
	if (nb_lines > 3) {
		log_printf("Warning: a balloon can have at most 3 lines, but the balloon %d have %d lines.\n"
			"The lines after the 3rd one will be ignored.\n", balloon_nb, nb_lines);
		nb_lines = 3;
	}
	for (cur_line = 0; cur_line < nb_lines; cur_line++) {
		const char *json_line = json_array_get_string(lines, cur_line);

		prepare_balloon(balloon, cur_line + 1, nb_lines);
		// Writing the replacement text
		PUT_CHAR(*file_out, *size_out, '"');
		strcpy(*file_out, json_line);
		MOVE_BUFF(*file_out, *size_out, strlen(json_line));
		if (line_break && cur_line + 1 == nb_lines) {
			PUT_CHAR(*file_out, *size_out, '\\');
		}
		PUT_CHAR(*file_out, *size_out, '"');
		PUT_CHAR(*file_out, *size_out, ',');

		// Writing the balloon name
		strcpy(*file_out, balloon);
		MOVE_BUFF(*file_out, *size_out, strlen(balloon));

		// Appenfing the end of the original text
		for (i = 0; i < size_in && file_in[i] != '\n' && *size_out > 0; i++) {
			**file_out = file_in[i];
			MOVE_BUFF(*file_out, *size_out, 1);
		}
		PUT_CHAR(*file_out, *size_out, '\n');
	}
}

int patch_pl(BYTE *file_inout, size_t size_out, size_t size_in, json_t *patch)
{
	BYTE* buffer_in;
	BYTE* file_in;
	size_t balloon;
	json_t* line_data;
	json_t* lines;
	BOOL need_to_copy_line;

#ifdef _DEBUG
	BYTE *file_inout_b = file_inout;
#endif

	// Make a copy of the input buffer
	buffer_in = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size_in);
	memcpy(buffer_in, file_inout, size_in);
	file_in = buffer_in;

	balloon = 1;

	for (; size_in > 0; next_line(&file_in, &size_in)) {
		need_to_copy_line = TRUE;
		if (!ignore_line(file_in, size_in)) {

			// The line contains replacable text
			line_data = json_object_numkey_get(patch, balloon);
			balloon++;
			if (json_is_object(line_data)) {

				// We have a corresponding entry in our jdiff file
				lines = json_object_get(line_data, "lines");
				if (json_is_array(lines)) {

					// We have an array of lines; we can replace our text.
					replace_line(file_in, size_in, &file_inout, &size_out, balloon, lines);
					need_to_copy_line = FALSE;

				}
				json_decref_safe(lines);
			}
			json_decref_safe(line_data);
		}

		if (need_to_copy_line) {
			copy_line(file_in, size_in, file_inout, size_out);
			next_line(&file_inout, &size_out);
		}
	}

	free(buffer_in);

#ifdef _DEBUG
	FILE* fd = fopen("out.pl", "wb");
	if (fd) {
		fwrite(file_inout_b, file_inout - file_inout_b, 1, fd);
		fclose(fd);
	}
#endif

	return 0;
}