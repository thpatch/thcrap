/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * On-the-fly th175 Tasofro json patcher
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"

static char *read_line(char *file, char *end)
{
	if (file >= end) {
		return nullptr;
	}

	while (file < end && *file != '\n') {
		file++;
	}
	return file;
}

static bool need_to_add_comma_to_line(char *start, char *end)
{
	while (end > start && (*end == '\r' || *end == '\n')) {
		end--;
	}
	while (end > start && (*end == ' ' || *end == '\t')) {
		end--;
	}
	if (end == start // Blank line
		|| *end == '{' || *end == ':') {
		return false;
	}
	else {
		return true;
	}
}

static void add_commas_in_input(char *file, size_t size)
{
	char *end = file + size;
	char *line_end;
	while (line_end = read_line(file, end)) {
		if (need_to_add_comma_to_line(file, line_end)) {
			*line_end = ',';
		}
		file = line_end + 1;
	}
}

static void skip_text_if_present(void*& buffer, size_t& size_in, size_t& size_out, const char *text)
{
	size_t text_len = strlen(text);
	if (size_in <= text_len) {
		return;
	}
	if (strncmp((const char *)buffer, text, text_len) != 0) {
		return;
	}

	buffer = (char*)buffer + text_len;
	size_in -= text_len;
	size_out -= text_len;
}

int patch_th175_json(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
	if (!patch) {
		return 0;
	}

	// Skip UTF-8 BOM
	skip_text_if_present(file_inout, size_in, size_out, "\xEF\xBB\xBF");
	// These files are close to normal JSON files, but they start with a "return" that is definitely
	// not valid JSON.
	skip_text_if_present(file_inout, size_in, size_out, "return");

	add_commas_in_input((char*)file_inout, size_in);

	json_t *json = json5_loadb(file_inout, size_in, nullptr);
	if (json == nullptr) {
		return 0;
	}

	json_object_update_recursive(json, patch);

	size_t new_size = json_dumpb(json, (char*)file_inout, size_out, JSON_COMPACT);
	if (new_size > size_out) {
		log_printf("[th175_json] Error: %s: buffer too small (need %zu, have %zu)", fn, new_size, size_out);
		memcpy(file_inout, "{}", 3);
		return 0;
	}
	size_out = new_size;
	((char*)file_inout)[size_out] = '\0';

	return 1;
}
