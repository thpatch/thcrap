/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * On-the-fly th175 pl patcher
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"
#include "mediawiki.h"

static const char *STAGE_NAME_COMMAND = u8",ステージ見出し,";

static void copy_line(const char*& file_in, const char *file_in_end, char*& file_out)
{
	const char *search_end = std::find(file_in, file_in_end, '\n');
	if (search_end != file_in_end) search_end++;
	file_out = std::copy(file_in, search_end, file_out);
	file_in = search_end;
}

static void copy_text_line(const char*& file_in, const char *file_in_end, char*& file_out)
{
	const char *search_end = std::find(file_in + 1, file_in_end, '"');
	if (search_end != file_in_end) search_end++;
	file_out = std::copy(file_in, search_end, file_out);
	file_in = search_end;
	copy_line(file_in, file_in_end, file_out);
}

static void patch_text_line(const char*& file_in, const char *file_in_end, char*& file_out, const std::string& rep)
{
	bool ends_with_pause = false;
	// Skip original text in input
	file_in = std::find(file_in + 1, file_in_end, '"');

	// If the text ends with an @, add it at the end of our replacement text
	if (*(file_in - 1) == '@') {
		ends_with_pause = true;
	}
	if (file_in != file_in_end) {
		// Skip the " at the end
		file_in++;
	}

	*file_out = '"'; file_out++;

	for (char c : rep) {
		if (c == '"') {
			*file_out = '"'; file_out++;
		}
		*file_out = c; file_out++;
	}

	if (ends_with_pause) {
		*file_out = '@'; file_out++;
	}
	*file_out = '"'; file_out++;
	copy_line(file_in, file_in_end, file_out);
}

static void patch_stage_name(const char*& file_in, const char *file_in_end, char*& file_out, json_t *rep_obj)
{
	if (!rep_obj) {
		copy_line(file_in, file_in_end, file_out);
		return;
	}

	json_t *value;
	std::string rep;
	json_flex_array_foreach_scoped(size_t, i, rep_obj, value) {
		if (!rep.empty()) {
			rep += "\n";
		}
		rep += json_string_value(value);
	}
	if (rep.empty()) {
		copy_line(file_in, file_in_end, file_out);
		return;
	}
	rep = parse_mediawiki(rep, mwdef_th175_stage_title);

	// Skip original text in input
	file_in = std::find(file_in, file_in_end, '\n');
	if (file_in != file_in_end) file_in++;

	strcpy(file_out, STAGE_NAME_COMMAND); file_out += strlen(STAGE_NAME_COMMAND);
	*file_out = '"'; file_out++;
	file_out = std::copy(rep.begin(), rep.end(), file_out);
	strcpy(file_out, "\"\r\n"); file_out += 3;
}

int patch_th175_pl(void *file_inout, size_t, size_t size_in, const char *, json_t *patch)
{
	if (!patch) {
		return 0;
	}

	auto file_in_storage = std::make_unique<char[]>(size_in);
	std::copy((char*)file_inout, ((char*)file_inout) + size_in, file_in_storage.get());

	const char *file_in = file_in_storage.get();
	const char *file_in_end = file_in + size_in;
	char *file_out = (char*)file_inout;

	unsigned int line_num = 1;
	while (file_in < file_in_end) {
		if (strncmp(file_in, STAGE_NAME_COMMAND, strlen(STAGE_NAME_COMMAND)) == 0) {
			patch_stage_name(file_in, file_in_end, file_out, json_object_get(patch, "stagename"));
			continue;
		}
		else if (*file_in != '"') {
			copy_line(file_in, file_in_end, file_out);
			continue;
		}

		json_t *rep = json_object_numkey_get(patch, line_num);
		line_num++;
		rep = json_object_get(rep, "lines");
		if (!rep) {
			copy_text_line(file_in, file_in_end, file_out);
			continue;
		}

		std::string rep_string;
		json_t *value;
		json_flex_array_foreach_scoped(size_t, i, rep, value) {
			if (!rep_string.empty()) {
				rep_string += '\n';
			}
			rep_string += json_string_value(value);
		}
		rep_string = parse_mediawiki(rep_string, mwdef_th135);
		patch_text_line(file_in, file_in_end, file_out, rep_string);
	}

#if 0
	std::filesystem::path dir = std::filesystem::absolute(fn);
	dir.remove_filename();
	std::filesystem::create_directories(dir);

	std::ofstream f(fn, std::ios::binary);
	f.write((char*)file_inout, size_out);
	f.close();
#endif

	return 1;
}
