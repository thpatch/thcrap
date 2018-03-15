/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#pragma once

#include <array>
#include "console.h"

// Writes [str] to a new file name [fn] in text mode.
int file_write_text(const char *fn, const char *str);

// Returns an array of patch selections. The engine's run configuration will
// contain all selected patches in a fully initialized state.
json_t* SelectPatchStack(json_t *repo_list);

// Shows a file write error and asks the user if they want to continue
int file_write_error(const char *fn);

#define DEFAULT_ANSWER 0x80

// Asks an optional [question] until one of the [answers] was entered, and
// returns the given answer in lowercase. This check is case-insensitive.
// Set the DEFAULT_ANSWER bit on one of the answers to automatically pick it
// if nothing was entered.
template <size_t N = 2> char Ask(
	const char *question,
	std::array<unsigned char, N> answers = { 'y', 'n' }
)
{
	char defopt = '\0';
	for(size_t i = 0; i < answers.size(); i++) {
		auto c = answers[i];
		if(c & DEFAULT_ANSWER) {
			assert(defopt == '\0' || !"Only one option can be the default");
			defopt = c & ~DEFAULT_ANSWER;
		}
		answers[i] = tolower(c & ~DEFAULT_ANSWER);
	}

	while(1) {
		char buf[2];
		if(question) {
			log_print(question);
			log_print(" ");
		}
		log_print("(");
		for(size_t i = 0; i < answers.size(); i++) {
			auto c = answers[i];
			if(i >= 1) {
				log_print("/");
			}
			if(c == defopt) {
				log_printf("[%c]", c);
			} else {
				log_printf("%c", c);
			}
		}
		log_print(") ");

		console_read(buf, sizeof(buf));
		char ret = tolower(buf[0]);
		if(ret == '\0') {
			ret = defopt;
		}

		for(auto c : answers) {
			if(ret == c) {
				return ret;
			}
		}
	}
	return '\0';
}

json_t* ConfigureLocateGames(const char *games_js_path);
