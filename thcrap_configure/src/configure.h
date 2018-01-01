/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#pragma once

#include <array>

// Yes, I know this is the wrong way, but wineconsole...
extern int wine_flag;

// Writes [str] to a new file name [fn] in text mode.
int file_write_text(const char *fn, const char *str);

// Returns an array of patch selections. The engine's run configuration will
// contain all selected patches in a fully initialized state.
json_t* SelectPatchStack(json_t *repo_list);

// Shows a file write error and asks the user if they want to continue
int file_write_error(const char *fn);

// Asks an optional [question] until one of the [answers] was entered, and
// returns the given answer in lowercase. This check is case-insensitive.
template <size_t N = 2> char Ask(
	const char *question,
	std::array<char, N> answers = { 'y', 'n' }
)
{
	while(1) {
		char buf[2];
		if(question) {
			log_print(question);
			log_print(" ");
		}
		log_print("(");
		for(size_t i = 0; i < answers.size(); i++) {
			auto c = tolower(answers[i]);
			if(i >= 1) {
				log_print("/");
			}
			log_printf("%c", c);
		}
		log_print(") ");

		console_read(buf, sizeof(buf));
		char ret = tolower(buf[0]);
		for(auto c : answers) {
			if(ret == tolower(c)) {
				return ret;
			}
		}
	}
	return '\0';
}

char* console_read(char *str, int n);
void cls(SHORT y);
void pause(void);

json_t* ConfigureLocateGames(const char *games_js_path);
