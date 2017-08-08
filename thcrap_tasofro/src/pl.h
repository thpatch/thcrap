/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * On-the-fly th145 pl patcher
  */

#pragma once

#include <string>
#include <vector>
#include <list>
#include <string.h>
#include <jansson.h>

namespace TasofroPl
{
	enum LineType
	{
		EMPTY,
		LABEL,
		COMMAND,
		TEXT
	};

	class ALine
	{
	protected:
		std::vector<std::string> fields;
		std::string comment;

	public:
		ALine(const std::vector<std::string>& fields, const std::string& comment = "");
		virtual ~ALine() {}

		virtual LineType getType() const = 0;
		virtual std::string toString() const;
		virtual bool isStaffroll() const;

		std::string unquote(const std::string& in) const;
		std::string quote(const std::string& in) const;

		// Access/update the members in fields.
		virtual const std::string& get(int n) const;
		virtual void set(int n, const std::string& value);
		virtual std::string& operator[](int n);
		virtual size_t size() const;
	};

	class Empty : public ALine
	{
	public:
		Empty(const std::vector<std::string>& fields, const std::string& comment = "");
		~Empty() {}

		LineType getType() const;
	};

	class Label : public ALine
	{
	private:
		std::string label;
	public:
		Label(const std::vector<std::string>& fields, const std::string& comment = "");
		~Label() {}

		LineType getType() const;
		const std::string& get() const;
		void set(const std::string& label);
	};

	class Command : public ALine
	{
	public:
		Command(const std::vector<std::string>& fields, const std::string& comment = "");
		~Command() {}

		LineType getType() const;
		bool isStaffroll() const;

		// Access/update the command name (index 0) or a parameter (index 1->size).
		const std::string& get(int n) const;
		void set(int n, const std::string& value);
		std::string& operator[](int n);
		size_t size() const;
	};

	class Text : public ALine
	{
	public:
		enum Syntax {
			UNKNOWN,
			STORY,
			ENDINGS,
			WIN
		};

	private:
		// Patcher state
		std::string owner;
		std::string balloonName;

		std::string last_char;
		Syntax syntax;
		bool is_staffroll;
		bool is_staffroll_last_balloon;

		int cur_line;
		int nb_lines;

		bool ignore_clear_balloon;
		bool balloon_add_suffix;
		bool quote_when_done;
		bool delete_when_done;

		// Functions used by the patcher
		bool parseCommand(json_t *patch, int json_line_num);
		void beginLine(std::list<ALine*> file, std::list<ALine*>::iterator it);
		void patchLine(const char *text, std::list<ALine*> file, std::list<ALine*>::iterator it);
		void endLine();

	public:
		Text(const std::vector<std::string>& fields, const std::string& comment = "", Syntax syntax = UNKNOWN);
		~Text() {}

		LineType getType() const;
		void patch(std::list<ALine*> file, std::list<ALine*>::iterator& file_it, const std::string& balloonOwner, json_t *patch);
	};

	ALine* readLine(const char*& file, size_t& size);
	void readField(const char *in, size_t& pos, size_t size, std::string& out);
	json_t *balloonNumberToLines(json_t *patch, size_t balloon_number);
}

int patch_pl(void *file_inout, size_t size_out, size_t size_in, json_t *patch);
