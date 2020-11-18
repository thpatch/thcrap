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

void TasofroPl::readField(const char *in, size_t& pos, size_t size, std::string& out)
{
	bool is_in_quote = false;

	for (; pos < size; pos++) {
		if (in[pos] == '"') {
			if (is_in_quote && (pos + 1 < size && in[pos + 1] == '"')) {
				out += in[pos];
				pos++;
			}
			else {
				is_in_quote = !is_in_quote;
			}
		}

		if (!is_in_quote) {
			if (in[pos] == ',' ||
				in[pos] == '#' ||
				in[pos] == '\n' ||
				(in[pos] == '\r' && pos + 1 < size && in[pos + 1] == '\n')) {
				return;
			}
		}

		out += in[pos];
	}
}

TasofroPl::ALine* TasofroPl::readLine(const char*& file, size_t& size)
{
	std::vector<std::string> fields;
	std::string comment;

	size_t pos = 0;
	std::string field;
	while (true) {
		field.clear();
		readField(file, pos, size, field);
		fields.push_back(field);
		if (pos == size || file[pos] != ',') {
			break;
		}
		pos++;
	}
	file += pos;
	size -= pos;

	if (size > 0 && *file == '#') {
		pos = 0;
		while (pos < size && file[pos] != '\n') {
			pos++;
		}
		if (pos > 0 && file[pos - 1] == '\r') {
			pos--;
		}
		comment.assign(file, pos);
		file += pos;
		size -= pos;
	}

	if (size > 0 && *file == '\r') {
		file++;
		size--;
	}
	if (size > 0) {
		file++;
		size--;
	}

	char first_char = '\0';
	for (size_t i = 0; i < fields[0].length(); ) {
		if (fields[0][i] == ' ' || fields[0][i] == '\t') {
			i++;
		}
		else if (i + 1 < fields[0].length() && fields[0][i] == '\\' && fields[0][i + 1] == 'n') {
			i += 2;
		}
		else {
			first_char = fields[0][i];
			break;
		}
	}

	switch (first_char) {
	case '\0':
		if (fields.size() > 1)
			return new Command(fields, comment);
		else
			return new Empty(fields, comment);
	case ':':
		return new Label(fields, comment);
	default:
		return AText::createText(fields, comment);
	}
}

TasofroPl::ALine* TasofroPl::readLineStrictEol(const char*& file, size_t& size)
{
	std::string line;

	while (size > 0 && *file != '\n') {
		line += *file;
		file++; size--;
	}
	if (size > 0) {
		file++; size--;
	}
	if (line.length() >= 1 && line[line.length() - 1] == '\r') {
		line.erase(line.length() - 1);
	}

	const char *line_ptr = line.c_str();
	size_t line_size = line.length();
	return readLine(line_ptr, line_size);
}



TasofroPl::LineType TasofroPl::Empty::getType() const
{
	return EMPTY;
}
TasofroPl::LineType TasofroPl::Label::getType() const
{
	return LABEL;
}
TasofroPl::LineType TasofroPl::Command::getType() const
{
	return COMMAND;
}
TasofroPl::LineType TasofroPl::AText::getType() const
{
	return TEXT;
}


TasofroPl::ALine::ALine(const std::vector<std::string>& fields, const std::string& comment)
	: fields(fields), comment(comment)
{}

std::string TasofroPl::ALine::toString() const
{
	std::string str;
	for (size_t i = 0; i < this->fields.size(); i++) {
		str += this->fields[i];
		if (i < this->fields.size() - 1) {
			str += ",";
		}
	}
	str += this->comment;
	return str;
}

bool TasofroPl::ALine::isStaffroll() const
{
	return false;
}

std::string TasofroPl::ALine::unquote(const std::string& in) const
{
	bool is_in_quote = false;
	std::string out;

	for (size_t pos = 0; pos < in.size(); pos++) {
		if (in[pos] == '"') {
			if (is_in_quote && (pos + 1 < in.size() && in[pos + 1] == '"')) {
				pos++;
			}
			else {
				is_in_quote = !is_in_quote;
				continue;
			}
		}

		out += in[pos];
	}
	return out;
}

std::string TasofroPl::ALine::quote(const std::string& in) const
{
	std::string out;

	out = '"';
	for (size_t i = 0; i < in.size(); i++) {
		if (in[i] == '"') {
			// If we're at the beginning of the line, the game engine will confuse our escaping with the surrounding quotes.
			// I'll put a space to ensure it doesn't.
			if (i == 0) {
				out += ' ';
			}
			out += "\"\"";
		}
		else if (in.compare(i, 7, "{{ruby|") == 0) {
			i += 7;
			out += "\\R[";
			while (i < in.size() && in.compare(i, 2, "}}") != 0) {
				out += in[i];
				i++;
			}
			out += ']';
			i++; // Actually i += 2, because it will be incremented once again in the for loop.
		}
		else {
			out += in[i];
		}
	}
	out += '"';

	return out;
}

const std::string& TasofroPl::ALine::get(int n) const
{
	return this->fields[n];
}

void TasofroPl::ALine::set(int n, const std::string& value)
{
	this->fields[n] = value;
}

std::string& TasofroPl::ALine::operator[](int n)
{
	return this->fields[n];
}

size_t TasofroPl::ALine::size() const
{
	return this->fields.size();
}



TasofroPl::Empty::Empty(const std::vector<std::string>& fields, const std::string& comment)
	: ALine(fields, comment)
{}



TasofroPl::Label::Label(const std::vector<std::string>& fields, const std::string& comment)
	: ALine(fields, comment), label(fields[0].substr(1))
{}

const std::string& TasofroPl::Label::get() const
{
	return this->label;
}

void TasofroPl::Label::set(const std::string& label)
{
	this->label = label;
	this->fields[0] = std::string(":") + label;
}



TasofroPl::Command::Command(const std::vector<std::string>& fields, const std::string& comment)
	: ALine(fields, comment)
{}

const std::string& TasofroPl::Command::get(int n) const
{
	return this->ALine::get(n + 1);
}

void TasofroPl::Command::set(int n, const std::string& value)
{
	this->ALine::set(n + 1, value);
}

std::string& TasofroPl::Command::operator[](int n)
{
	return this->ALine::operator[](n + 1);
}

size_t TasofroPl::Command::size() const
{
	return this->ALine::size() - 1;
}

bool TasofroPl::Command::isStaffroll() const
{
	return this->size() == 2 && this->get(0) == "Function" && this->unquote(this->get(1)) == "::story.BeginStaffroll();";
}



TasofroPl::AText *TasofroPl::AText::createText(const std::vector<std::string>& fields, const std::string& comment, Syntax syntax)
{
	if (syntax == UNKNOWN) {
		if (fields.size() > 1) {
			syntax = STORY;
		}
		else {
			syntax = ENDINGS;
		}
	}

	switch (syntax) {
	case STORY:
		if (game_id < TH155) {
			return new StoryText(fields, comment);
		}
		else {
			bool strict_eol = json_boolean_value(json_object_get(runconfig_json_get(), "pl_parsing_strict_eol"));
			if (strict_eol) {
				return new Th155_110StoryText(fields, comment);
			}
			else {
				return new Th155StoryText(fields, comment);
			}
		}
	case ENDINGS:
		return new EndingText(fields, comment);
	case WIN:
		return new WinText(fields, comment);
	default:
		return nullptr;
	}
}

TasofroPl::AText::AText(const std::vector<std::string>& fields, const std::string& comment)
	: ALine(fields, comment)
{
}

TasofroPl::StoryText::StoryText(const std::vector<std::string>& fields, const std::string& comment)
	: AText(fields, comment)
{
}

TasofroPl::Th155StoryText::Th155StoryText(const std::vector<std::string>& fields, const std::string& comment)
	: StoryText(fields, comment)
{
}

TasofroPl::Th155_110StoryText::Th155_110StoryText(const std::vector<std::string>& fields, const std::string& comment)
	: Th155StoryText(fields, comment)
{
}

TasofroPl::EndingText::EndingText(const std::vector<std::string>& fields, const std::string& comment)
	: AText(fields, comment)
{
}

TasofroPl::WinText::WinText(const std::vector<std::string>& fields, const std::string& comment)
	: AText(fields, comment)
{
}

void TasofroPl::AText::patch(std::list<ALine*>& file, std::list<ALine*>::iterator& file_it, const std::string& balloonOwner, json_t *patch)
{
	this->fields[0] = this->unquote(this->fields[0]);
	this->owner = balloonOwner;
	this->cur_line = 1;
	this->nb_lines = 0;
	this->is_first_balloon = true;

	if (this->fields[0].length() >= 2 && this->fields[0].compare(this->fields[0].length() - 2, 2, "\\.") == 0)
		this->last_char = "\\.";
	else if (this->fields[0].length() >= 1 && (this->fields[0].back() == '\\' || this->fields[0].back() == '@'))
		this->last_char = this->fields[0].back();

	this->_patchInit(file, file_it);

	this->fields[0] = "";
	size_t json_line_num;
	json_t *json_line;
	json_array_foreach(patch, json_line_num, json_line) {
		if (this->parseCommand(patch, json_line_num) == true) {
			continue;
		}

		this->beginLine(file, file_it);
		this->patchLine(json_string_value(json_line), file, file_it);
		this->endLine();
	}

	this->_patchExit(file, file_it);
}

void TasofroPl::StoryText::_patchInit(std::list<ALine*>&, std::list<ALine*>::iterator&)
{
	this->balloonName = this->fields[1];
}

void TasofroPl::EndingText::_patchInit(std::list<ALine*>& file, std::list<ALine*>::iterator& file_it)
{
	this->is_staffroll = false;

	std::list<ALine*>::iterator next_it = file_it;
	while (next_it != file.end() && (*next_it)->getType() == TEXT &&
		!(this->fields[0].length() >= 1 && this->fields[0].back() == '\\')) {
		++next_it;
	}
	if (next_it != file.end() && (*next_it)->isStaffroll()) {
		this->is_staffroll = true;
	}
}

void TasofroPl::WinText::_patchInit(std::list<ALine*>&, std::list<ALine*>::iterator&)
{
	this->balloonName = this->fields[1];
}

void TasofroPl::AText::_patchExit(std::list<ALine*>&, std::list<ALine*>::iterator&)
{
}

void TasofroPl::StoryText::_patchExit(std::list<ALine*>& file, std::list<ALine*>::iterator& file_it)
{
	file_it = file.erase(file_it);
	--file_it;
	delete this;
}

void TasofroPl::Th155StoryText::_patchExit(std::list<ALine*>&, std::list<ALine*>::iterator&)
{
	this->fields[0] = this->quote(this->fields[0]);
}

void TasofroPl::EndingText::_patchExit(std::list<ALine*>& file, std::list<ALine*>::iterator& file_it)
{
	if (this->is_staffroll) {
		++file_it;
		while ((*file_it)->getType() == TEXT) {
			file_it = file.erase(file_it);
		}
		++file_it;
		file_it = file.erase(file_it);
		--file_it;
		--file_it;
	}
	this->fields[0] = this->quote(this->fields[0]);
}



bool TasofroPl::AText::parseCommand(json_t *patch, int json_line_num)
{
	const char *line;

	line = json_array_get_string(patch, json_line_num);
	if (strncmp(line, "<balloon", 8) == 0) {
		const char *balloon_end = strchr(line + 9, '>');
		if (line[8] == '$' && balloon_end && *balloon_end) {
			this->balloonName.assign(line + 9, balloon_end - (line + 9));
		}
		this->cur_line = 1;
		this->nb_lines = 0;
		this->is_first_balloon = false;
		return true;
	}

	std::string balloon_size;
	if (this->balloonName.size() >= 4) {
		balloon_size = this->balloonName.substr(1, 4);
	}
	if (this->balloonName.size() > 5) {
		this->balloonName.erase(5);
	}

	if (this->nb_lines == 0) {
		unsigned int i;
		for (i = json_line_num + 1; i < json_array_size(patch); i++) {
			line = json_array_get_string(patch, i);
			if (strncmp(line, "<balloon", 8) == 0) {
				break;
			}
		}
		this->nb_lines = i - json_line_num;

		// Try to see if we are in the last balloon.
		this->is_last_balloon = false;
		unsigned int j;
		for (j = i + 1; j < json_array_size(patch); j++) {
			line = json_array_get_string(patch, j);
			if (strncmp(line, "<balloon", 8) != 0) {
				break;
			}
		}
		if (j >= json_array_size(patch)) {
			this->is_last_balloon = true;
		}

		// If the balloon is too small for our text to fit, expand it automatically.
		if (this->nb_lines > 1 && balloon_size == "05x2") {
			this->balloonName.replace(1, 4, "11x2");
		}
		if (this->nb_lines > 2 && balloon_size == "11x2") {
			this->balloonName.replace(1, 4, "15x3");
		}
	}

	return false;
}

bool TasofroPl::StoryText::parseCommand(json_t *patch, int json_line_num)
{
	bool ret = this->AText::parseCommand(patch, json_line_num);
	if (ret == false) {
		this->balloonName += "_1_1";
		this->balloonName[6] = this->cur_line + '0';
		this->balloonName[8] = this->nb_lines + '0';
	}
	return ret;
}

bool TasofroPl::Th155StoryText::parseCommand(json_t *patch, int json_line_num)
{
	// Overwrite the StoryText overwrite and go back to the standard behavior
	return this->AText::parseCommand(patch, json_line_num);
}

void TasofroPl::StoryText::beginLine(std::list<ALine*>& file, const std::list<ALine*>::iterator& it)
{
	if (this->is_first_balloon == false && this->cur_line == 1) {
		file.insert(it, new Command(std::vector<std::string>({
			"",
			"ClearBalloon",
			this->owner
			})));
	}
}

void TasofroPl::Th155_110StoryText::beginLine(std::list<ALine*>& file, const std::list<ALine*>::iterator& it)
{
	if (this->is_first_balloon == false && this->cur_line == 1) {
		this->fields[0] = this->quote(this->fields[0]);
		file.insert(it, new Th155_110StoryText(this->fields));
		this->fields[0] = "";
	}
}

void TasofroPl::EndingText::beginLine(std::list<ALine*>& file, const std::list<ALine*>::iterator& it)
{
	if (this->is_first_balloon == false && this->cur_line == 1) {
		this->fields[0] = this->quote(this->fields[0]);
		file.insert(it, new EndingText(this->fields));
		this->fields[0] = "";
	}

	if (this->is_staffroll) {
		if (game_id >= TH145 && this->is_last_balloon) {
			this->last_char = "";
		}
		else {
			this->last_char = "\\";
		}
	}
}

void TasofroPl::WinText::beginLine(std::list<ALine*>& file, const std::list<ALine*>::iterator& it)
{
	if (this->is_first_balloon == false && this->cur_line == 1) {
		file.insert(it, new WinText(this->fields));
		this->fields[0] = "";
	}
}

void TasofroPl::AText::patchLine(const char *text, std::list<ALine*>& file, const std::list<ALine*>::iterator& it)
{
	std::string formattedText = text;
	if (this->cur_line == this->nb_lines) {
		if (this->last_char.empty() == false) {
			formattedText += this->last_char;
		}
		else if (this->is_last_balloon == false) {
			formattedText += "\\";
		}
	}

	this->_patchLine(formattedText, file, it);
}

void TasofroPl::StoryText::_patchLine(std::string& text, std::list<ALine*>& file, const std::list<ALine*>::iterator& it)
{
	std::vector<std::string> new_fields = this->fields;
	new_fields[0] = this->quote(text);
	new_fields[1] = this->balloonName;
	file.insert(it, new StoryText(new_fields));
}

void TasofroPl::Th155StoryText::_patchLine(std::string& text, std::list<ALine*>&, const std::list<ALine*>::iterator&)
{
	if (this->cur_line != this->nb_lines) {
		text += "\\n";
	}
	this->fields[0] += text;
	this->fields[1] = this->balloonName;
}

void TasofroPl::EndingText::_patchLine(std::string& text, std::list<ALine*>&, const std::list<ALine*>::iterator&)
{
	if (this->cur_line != this->nb_lines) {
		text += "\\n";
	}
	if (this->is_staffroll) {
		size_t break_pos;
		while ((break_pos = text.find("\\.")) != std::string::npos) {
			text.erase(break_pos, 2);
		}
	}
	this->fields[0] += text;
}

void TasofroPl::WinText::_patchLine(std::string& text, std::list<ALine*>&, const std::list<ALine*>::iterator&)
{
	if (this->cur_line != this->nb_lines) {
		text += "\\n";
	}
	this->fields[0] += text;
	this->fields[1] = this->balloonName;
}

void TasofroPl::AText::endLine()
{
	if (this->cur_line != 3) {
		this->cur_line++;
	}
	else {
		this->cur_line = 1;
		this->is_first_balloon = false;
	}
}

void TasofroPl::EndingText::endLine()
{
	int textbox_size = 3;
	if (game_id >= TH155) {
		textbox_size = 5;
	}

	if (this->cur_line != textbox_size) {
		this->cur_line++;
	}
	else {
		this->cur_line = 1;
		this->is_first_balloon = false;
	}
}



json_t *TasofroPl::balloonNumberToLines(json_t *patch, size_t balloon_number)
{
	json_t *json_line_data;
	json_t *json_lines;

	json_line_data = json_object_numkey_get(patch, balloon_number);
	if (!json_is_object(json_line_data)) {
		return nullptr;
	}

	json_lines = json_object_get(json_line_data, "lines");
	if (!json_is_array(json_lines)) {
		return nullptr;
	}

	return json_lines;
}

static bool storyLineIsFinished(TasofroPl::ALine& line)
{
	std::string& text = line[0];
	return text[text.size() - 1] == '\\';
}

static void stickStoryLines(std::list<TasofroPl::ALine*>& lines)
{
	for (std::list<TasofroPl::ALine*>::iterator it = lines.begin(); it != lines.end(); ++it) {
		if ((*it)->getType() == TasofroPl::TEXT) {
			while (storyLineIsFinished(**it) == false) {
				std::list<TasofroPl::ALine*>::iterator next = it;
				++next;
				if ((*next)->getType() == TasofroPl::TEXT) {
					(**it)[0] += "\n" + (**next)[0];
					lines.erase(next);
				}
				else if ((*next)->getType() == TasofroPl::EMPTY) {
					(**it)[0] += "\n";
					lines.erase(next);
				}
				else {
					break;
				}
			}
		}
	}
}

int patch_pl(void *file_inout, size_t size_out, size_t size_in, const char *, json_t *patch)
{
	if (!patch) {
		return 0;
	}

	std::list<TasofroPl::ALine*> lines;
	const char *file_in = (const char*)file_inout;
	char *file_out = (char*)file_inout;
	bool strict_eol = json_boolean_value(json_object_get(runconfig_json_get(), "pl_parsing_strict_eol"));

	while (size_in > 0) {
		if (strict_eol) {
			// th155 v1.10
			lines.push_back(TasofroPl::readLineStrictEol(file_in, size_in));
		}
		else {
			// th155 older versions, or just th135/th145.
			lines.push_back(TasofroPl::readLine(file_in, size_in));
		}
	}
	if (game_id == TH155) {
		// Don't split ending lines by line endings.
		// Instead, try to do a semantic splitting by sticking together the lines belonging to the same text box.
		stickStoryLines(lines);
	}

	std::string balloonOwner;
	size_t balloon_number = 1;
	for (std::list<TasofroPl::ALine*>::iterator it = lines.begin(); it != lines.end(); ++it) {
		TasofroPl::ALine *line = *it;
		if (line->getType() == TasofroPl::COMMAND) {
			TasofroPl::Command& command = dynamic_cast<TasofroPl::Command&>(*line);
			if (command[0] == "SetFocus") {
				balloonOwner = command[1];
			}
		}
		if (line->getType() != TasofroPl::TEXT) {
			continue;
		}

		json_t *json_lines = TasofroPl::balloonNumberToLines(patch, balloon_number);
		balloon_number++;
		if (json_lines == nullptr) {
			continue;
		}
		dynamic_cast<TasofroPl::AText*>(line)->patch(lines, it, balloonOwner, json_lines);
	}

#if 0
	size_t size_out_orig = size_out;
#endif

	std::string str;
	for (TasofroPl::ALine* line : lines) {
		str = line->toString();
		if (str.size() > size_out + 3) {
			log_print("Output file too small\n");
			break;
		}
		memcpy(file_out, str.c_str(), str.size());
		file_out += str.size();
		size_out -= str.size();
		memcpy(file_out, "\r\n", 2);
		file_out += 2;
		size_out -= 2;
	}
	*file_out = '\0';

#if 0
	file_write(fn, file_inout, size_out_orig - size_out);
#endif

	return 1;
}
