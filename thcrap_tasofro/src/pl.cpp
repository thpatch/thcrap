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
#include "pl.h"

#define MOVE_BUFF(buff, size, n) (buff) += n; (size) -= n;
#define PUT_CHAR(buff, size, c) *(buff) = c; MOVE_BUFF(buff, size, 1);
#define PUT_STR(buff, size, str) strcpy(buff, str); MOVE_BUFF(buff, size, strlen(str));

static balloon_t balloon;

void balloon_set_character(char* line, size_t size)
{
	unsigned int i;

	for (i = 0; i < size && line[i] != '#' && line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '\n'; i++);
	if (i < size) {
		line[i] = '\0';
		strcpy(balloon.owner, line);
		line[i] = '\t';
	}
}

balloon_t *balloon_get()
{
	balloon.cur_line = 1;
	balloon.nb_lines = 0;
	return &balloon;
}

static void next_line(char **file, size_t *size)
{
	while (*size > 0 && **file != '\n') {
		MOVE_BUFF(*file, *size, 1);
	}
	if (*size > 0) {
		MOVE_BUFF(*file, *size, 1);
	}
}

static BOOL ignore_line(char *file, size_t size)
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
		if (strncmp(file, ",SetFocus,", 10) == 0) {
			balloon_set_character(file + 10, size - 10);
		}
		return TRUE;
	}
	return FALSE;
}

static void copy_line(char *file_in, size_t size_in, char *file_out, size_t size_out)
{
	size_t i;

	for (i = 0; i < size_in && i < size_out && file_in[i] != '\n'; i++) {
		file_out[i] = file_in[i];
	}
	if (i < size_out) {
		file_out[i] = '\n';
	}
}

static int balloon_gen_name(balloon_t *balloon, int cur_line, json_t *lines)
{
	// TODO: actually look for a nice balloon size
	const char *line;

	line = json_array_get_string(lines, cur_line);
	if (strncmp(line, "<balloon", 8) == 0) {
		if (line[8] == '$') {
			strncpy(balloon->name, line + 9, 5);
		}
		balloon->cur_line = 0;
		balloon->nb_lines = 0;
		return 0;
	}

	if (balloon->nb_lines == 0) {
		unsigned int i;
		for (i = cur_line + 1; i < json_array_size(lines); i++) {
			line = json_array_get_string(lines, i);
			if (strncmp(line, "<balloon", 8) == 0) {
				break;
			}
		}
		balloon->nb_lines = i - cur_line;

		// If the balloon is too small for our text to fit, expand it automatically.
		if (balloon->nb_lines > 1 && strncmp(balloon->name + 1, "05x2", 4) == 0) {
			strncpy(balloon->name + 1, "11x2", 3);
		}
		if (balloon->nb_lines > 2 && strncmp(balloon->name + 1, "11x2", 4) == 0) {
			strncpy(balloon->name + 1, "15x3", 4);
		}
	}

	strcpy(balloon->name + 5, "_1_1");
	balloon->name[6] = balloon->cur_line + '0';
	balloon->name[8] = balloon->nb_lines + '0';
	return 1;
}

static void put_line(char **file_out, size_t *size_out, const char *line)
{
	int i;

	for (i = 0; line[i]; i++) {
		if (line[i] == '"') {
			// If we're at the beginning of the line, the game engine will confuse our escaping with the surrounding quotes.
			// I'll put a space to ensure it doesn't.
			if (i == 0) {
				PUT_CHAR(*file_out, *size_out, ' ');
			}
			PUT_STR(*file_out, *size_out, "\"\"")
		}
		else if (strncmp(line + i, "{{ruby|", 7) == 0) {
			i += 7;
			PUT_STR(*file_out, *size_out, "\\R[");
			while (line[i] && strncmp(line + i, "}}", 2) != 0) {
				PUT_CHAR(*file_out, *size_out, line[i]);
				i++;
			}
			PUT_CHAR(*file_out, *size_out, ']');
			if (line[i]) {
				i++;
			}
		}
		else {
			PUT_CHAR(*file_out, *size_out, line[i]);
		}
	}
}

static void put_line_story(char *file_in, size_t size_in, char **file_out, size_t *size_out, json_t *lines, balloon_t *balloon)
{
	unsigned int cur_line = 0;
	unsigned int nb_lines = json_array_size(lines);
	int ignore_clear_balloon = 1;
	unsigned int i;

	for (; cur_line < nb_lines; cur_line++, balloon->cur_line++) {
		if (balloon_gen_name(balloon, cur_line, lines) == 0)
			continue;
		const char *json_line = json_array_get_string(lines, cur_line);

		// Add the "ClearBalloon" command if needed
		if (ignore_clear_balloon == 0 && balloon->cur_line == 1) {
			PUT_STR(*file_out, *size_out, ",ClearBalloon,");
			PUT_STR(*file_out, *size_out, balloon->owner);
			PUT_CHAR(*file_out, *size_out, '\r');
			PUT_CHAR(*file_out, *size_out, '\n');
		}
		if (ignore_clear_balloon == 1) {
			ignore_clear_balloon = 0;
		}

		// Writing the replacement text
		PUT_CHAR(*file_out, *size_out, '"');
		put_line(file_out, size_out, json_line);
		if (balloon->last_char == '\\' && balloon->cur_line == balloon->nb_lines) {
			PUT_CHAR(*file_out, *size_out, '\\');
		}
		PUT_CHAR(*file_out, *size_out, '"');

		PUT_CHAR(*file_out, *size_out, ',');

		// Writing the balloon name
		PUT_STR(*file_out, *size_out, balloon->name);

		// Appenfing the end of the original text
		for (i = 0; i < size_in && file_in[i] != '\n' && *size_out > 0; i++) {
			PUT_CHAR(*file_out, *size_out, file_in[i]);
		}
		PUT_CHAR(*file_out, *size_out, '\n');

		// Ensure we won't make a 4-lines balloon
		if (balloon->cur_line == 3) {
			balloon->cur_line = 0;
		}
	}
}

static int put_line_ending(char *file_in, size_t size_in, char **file_out, size_t *size_out, json_t *lines, balloon_t *balloon)
{
	unsigned int cur_line = 0;
	unsigned int balloon_cur_line = 1;
	unsigned int nb_lines = json_array_size(lines);

	for (; cur_line < nb_lines; cur_line++, balloon_cur_line++) {
		const char *json_line = json_array_get_string(lines, cur_line);

		if (strncmp(json_line, "<balloon", 8) == 0) {
			if (balloon_cur_line != 1) {
				if (balloon->last_char == '\\') {
					PUT_CHAR(*file_out, *size_out, '\\');
				}
				PUT_STR(*file_out, *size_out, "\"\n");
			}
			balloon_cur_line = 0;
			continue;
		}
		if (balloon_cur_line == 1) {
			PUT_CHAR(*file_out, *size_out, '"');
		}
		if (balloon->last_char == '@' && cur_line == nb_lines - 1) {
			if (cur_line != 0) {
				// If the input already contains a pause mark, remove it.
				if (strncmp(*file_out - 2, "\\.", 2) == 0) {
					*file_out -= 2;
					*size_out += 2;
				}
				PUT_STR(*file_out, *size_out, "@\"\n");
			}
			else {
				// Remove the quote
				(*file_out)--;
				(*size_out)++;
			}
			PUT_STR(*file_out, *size_out, ",Function,\"::story.BeginStaffroll();\"\n\"");
		}
		if (balloon_cur_line > 1) {
			PUT_STR(*file_out, *size_out, "\\n");
		}
		put_line(file_out, size_out, json_line);
	}
	if (balloon_cur_line > 1) {
		if (balloon->last_char == '\\') {
			PUT_CHAR(*file_out, *size_out, '\\');
		}
		PUT_STR(*file_out, *size_out, "\"\n");
	}
	return balloon->last_char == '@';
}

/**
  * Replaces a line in a PL file (for both story mode and endings).
  * If we are in an ending file and the caller needs to skip the story.BeginStaffroll call, this function returns 1.
  * Otherwise, it returns 0.
  */
static int replace_line(char *file_in, size_t size_in, char **file_out, size_t *size_out, unsigned int balloon_nb, json_t *lines)
{
	balloon_t *balloon = balloon_get();
	unsigned int i;

	// Skip the original text
	int is_in_quote = 0;
	while (size_in > 0) {
		if (is_in_quote == 0 && (*file_in == '\r' || *file_in == '\n' || *file_in == ',')) {
			break;
		}
		if (*file_in == '"') {
			if (is_in_quote == 1 && file_in[1] == '"') {
				MOVE_BUFF(file_in, size_in, 1);
			}
			else {
				is_in_quote = !is_in_quote;
			}
		}
		MOVE_BUFF(file_in, size_in, 1);
	}
	if (file_in[-1] != '"') {
		balloon->last_char = file_in[-1];
	}
	else {
		balloon->last_char = file_in[-2];
	}

	if (*file_in == ',') {
		balloon->is_ending = 1;
		MOVE_BUFF(file_in, size_in, 1);
		// Copy the balloon name from the original file
		for (i = 0; size_in > 0 && *file_in != '\n' && *file_in != ',' && i < 10; i++, file_in++, size_in--) {
			balloon->name[i] = *file_in;
		}
	}
	else {
		balloon->is_ending = 0;
	}

	if (balloon->is_ending) {
		put_line_story(file_in, size_in, file_out, size_out, lines, balloon);
		return 0;
	}
	else {
		return put_line_ending(file_in, size_in, file_out, size_out, lines, balloon);
	}
}

int patch_pl(void *_file_inout, size_t size_out, size_t size_in, json_t *patch)
{
	char* file_inout = (char*)_file_inout;
	char* buffer_in;
	char* file_in;
	size_t balloon;
	json_t* line_data;
	json_t* lines;
	BOOL need_to_copy_line;

#ifdef _DEBUG
	char *file_inout_b = file_inout;
#endif

	// Make a copy of the input buffer
	buffer_in = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size_in);
	memcpy(buffer_in, file_inout, size_in);
	file_in = buffer_in;

	balloon = 1;

	while (size_in > 0) {
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
					int skip_staff_roll = replace_line(file_in, size_in, &file_inout, &size_out, balloon, lines);
					need_to_copy_line = FALSE;

					next_line(&file_in, &size_in);
					/**
					  * If we are in the last line of the endings:
					  * - skip the end of the line;
					  * - skip the story.BeginStaffroll call;
					  * - skip the last line of test.
					  */
					if (skip_staff_roll)
					{
						while (ignore_line(file_in, size_in) == FALSE) {
							next_line(&file_in, &size_in);
						}
						next_line(&file_in, &size_in);
						next_line(&file_in, &size_in);
					}
				}
				json_decref_safe(lines);
			}
			json_decref_safe(line_data);
		}

		if (need_to_copy_line) {
			copy_line(file_in, size_in, file_inout, size_out);
			next_line(&file_in, &size_in);
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