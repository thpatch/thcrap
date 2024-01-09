/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * On-the-fly th175 pl endings patcher
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"

static std::string read_line(const char*& file_in, const char *file_in_end)
{
	const char *search_end = std::find(file_in, file_in_end, '\n');
	if (search_end != file_in_end) search_end++;
	auto line = std::string(file_in, search_end);
	file_in = search_end;
	return line;
}

static void write_line(const char *line, char*& file_out)
{
	file_out = std::copy(line, line + strlen(line), file_out);
}

static void write_line(const std::string& line, char*& file_out)
{
	file_out = std::copy(line.begin(), line.end(), file_out);
}

static void write_msg(const std::vector<std::string>& msg, char*& file_out)
{
	for (const auto& it : msg) {
		write_line(it, file_out);
	}
}

static void copy_line(const char*& file_in, const char *file_in_end, char*& file_out)
{
	write_line(read_line(file_in, file_in_end), file_out);
}

static bool is_ed_msg(const char *begin, const char *end)
{
	constexpr char msg_cmd[] = ",ED_MSG,";
	constexpr size_t msg_cmd_len = sizeof(msg_cmd) - 1;

	if (begin + msg_cmd_len >= end) {
		return false;
	}
	return memcmp(begin, msg_cmd, msg_cmd_len) == 0;
}

std::vector<std::string> split(std::string str, char delim)
{
	std::vector<std::string> out;

	auto it = str.begin();
	while (it != str.end()) {
		auto delim_pos = std::find(it, str.end(), delim);
		out.emplace_back(it, delim_pos);

		it = delim_pos;
		if (it != str.end()) {
			++it;
		}
	}

	return out;
}

static void patch_msg(const std::vector<std::string>& msg, const std::vector<std::string>& rep, char*& file_out)
{
	auto last_line_split = split(*msg.rbegin(), ',');

	for (auto it = rep.begin(); it != rep.end(); ++it) {
		write_line(",ED_MSG,", file_out);
		write_line(*it, file_out);

		if (it + 1 == rep.end() && last_line_split.size() > 3) {
			for (auto it2 = last_line_split.begin() + 3; it2 != last_line_split.end(); ++it2) {
				write_line(",", file_out);
				write_line(*it2, file_out);
			}
		}
		else {
			write_line("\r\n", file_out);
		}
	}
}

static std::string escape(const char *str)
{
	std::string out;

	out += '"';
	while (*str) {
		/*
		 * The texts will go through the macro interpreter, replacing
		 * ED_MSG with ::scene.contents[""ending""].set_text(""%%1%%"", ""%%2%%"");
		 * (the ED_MSG macro is defined in data/event/script/lib/util.pl),
		 * where every double quote need to be escaped with another double quote,
		 * then through the Squirrel compiler, where double quotes need to be
		 * escaped with backslashs, which means " will become \"" (and of course,
		 * in the thcrap source code here, the backslash and the 2 quotes will go
		 * through the C compiler and need to be escaped once more).
		 */
		if (*str == '"') {
			out += "\\\"\"";
		}
		else {
			out += *str;
		}
		str++;
	}
	out += '"';

	return out;
}

bool is_msg_empty(const std::vector<std::string>& msg)
{
	for (auto& it : msg) {
		auto line_split = split(it, ',');
		if (line_split.size() >= 3 && line_split[2].find_first_not_of(" \t\r\n\"") != std::string::npos) {
			return false;
		}
	}
	return true;
}

int patch_th175_pl_ed(void *file_inout, size_t, size_t size_in, const char *, json_t *patch)
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
		if (!is_ed_msg(file_in, file_in_end)) {
			copy_line(file_in, file_in_end, file_out);
			continue;
		}

		std::vector<std::string> msg;
		while (is_ed_msg(file_in, file_in_end)) {
			msg.push_back(read_line(file_in, file_in_end));
		}
		if (is_msg_empty(msg)) {
			write_msg(msg, file_out);
			continue;
		}

		json_t *rep = json_object_numkey_get(patch, line_num);
		line_num++;
		rep = json_object_get(rep, "lines");
		if (!rep) {
			write_msg(msg, file_out);
			continue;
		}

		std::vector<std::string> rep_msg;
		json_t *value;
		json_flex_array_foreach_scoped(size_t, i, rep, value) {
			rep_msg.push_back(escape(json_string_value(value)));
		}
		patch_msg(msg, rep_msg, file_out);
	}

	*file_out = '\0';

#if 0
	std::filesystem::path dir = std::filesystem::absolute(fn);
	dir.remove_filename();
	std::filesystem::create_directories(dir);

	std::ofstream f(fn, std::ios::binary);
	f.write((char*)file_inout, file_out - (char*)file_inout);
	f.close();
#endif

	return 1;
}
