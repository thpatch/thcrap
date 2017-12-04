/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * On-the-fly th105 cv0 patcher
  */

#pragma once

#include <string>
#include <vector>
#include <list>
#include <string.h>
#include <jansson.h>

namespace TasofroCv0
{
	enum LineType
	{
		EMPTY,
		COMMAND,
		TEXT
	};

	class ALine
	{
	public:
		ALine();
		virtual ~ALine() {}

		virtual LineType getType() const = 0;
		virtual std::string toString() const = 0;

		static std::string readLine(const char*& file, size_t& size);

		std::string unescape(const std::string& in) const;
		std::string escape(const std::string& in) const;
	};

	class Empty : public ALine
	{
	private:
		std::string content; // whitespaces and comments

	public:
		Empty();
		~Empty() {}

		LineType getType() const;
		std::string toString() const;

		static Empty* read(const char*& file, size_t& size);
	};

	class Command : public ALine
	{
	private:
		std::string content; // Should be an array of fields, but we don't need to parse commands for now, so we don't need it.

	public:
		Command();
		~Command() {}

		LineType getType() const;
		std::string toString() const;

		static Command* read(const char*& file, size_t& size);
	};

	class Text : public ALine
	{
	private:
		std::string content;

		// Patcher state
		int cur_line;
		int nb_lines;

		// Functions used by the patcher
		bool parseCommand(json_t *patch, int json_line_num);
		void beginLine(std::list<ALine*>& file, const std::list<ALine*>::iterator& it);
		void patchLine(const char *text, std::list<ALine*>& file, const std::list<ALine*>::iterator& it);
		void endLine();

	public:
		Text();
		Text(const std::string& content);
		~Text() {}

		LineType getType() const;
		std::string toString() const;
		void patch(std::list<ALine*>& file, std::list<ALine*>::iterator& file_it, json_t *patch);

		static Text* read(const char*& file, size_t& size);
	};

	LineType guessLineType(const char* file, size_t size);
	ALine* readLine(const char*& file, size_t& size);
	json_t *balloonNumberToLines(json_t *patch, size_t balloon_number);
}

int patch_cv0(void *file_inout, size_t size_out, size_t size_in, const char*, json_t *patch);
