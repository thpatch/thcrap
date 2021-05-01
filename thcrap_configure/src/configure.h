/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#pragma once

#include <array>
#include <list>
#include <map>
#include <vector>
#include <string>
#include <string_view>
#include <stdio.h>
#include "console.h"

typedef std::list<patch_desc_t> patch_sel_stack_t;

// Writes [str] to a new file name [fn] in text mode.
int file_write_text(const char *fn, const char *str);

// Returns an array of patch selections. The engine's run configuration will
// contain all selected patches in a fully initialized state.
patch_sel_stack_t SelectPatchStack(repo_t **repo_list);

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
		log_print(")\n");

		wint_t ret = towlower(console_read()[0]);
		if(ret == L'\0')
			ret = defopt;

		for(auto c : answers)
			if(ret == (wint_t)c)
				return c;
	}
	return '\0';
}

inline std::string to_utf8(std::wstring_view wstr)
{
	std::string str(wstr.size() * UTF8_MUL, '\0');
	StringToUTF8(str.data(), wstr.data(), str.size());
	str.resize(strlen(str.c_str()));
	return str;
}
inline std::wstring to_utf16(std::string_view str)
{
	std::wstring wstr(str.size(), '\0');
	StringToUTF16(wstr.data(), str.data(), wstr.size());
	wstr.resize(wcslen(wstr.c_str()));
	return wstr;
}
inline std::string stringf(const char *format, va_list va)
{
	int len = vsnprintf(NULL, 0, format, va);
	std::string str(len, '\0');
	vsnprintf(str.data(), str.size() + 1, format, va);
	str.resize(strlen(str.c_str()));
	return str;
}
inline std::string stringf(const char *format, ...)
{
	va_list va;
	va_start(va, format);
	std::string str = stringf(format, va);
	va_end(va);
	return str;
}
inline std::wstring wstringf(const wchar_t *format, va_list va)
{
	size_t len = _vscwprintf(format, va);
	std::wstring str(len, L'\0');
	vswprintf(str.data(), str.size() + 1, format, va);
	str.resize(wcslen(str.c_str()));
	return str;
}
inline std::wstring wstringf(const wchar_t *format, ...)
{
	va_list va;
	va_start(va, format);
	std::wstring str = wstringf(format, va);
	va_end(va);
	return str;
}

json_t* ConfigureLocateGames(const char *games_js_path);
