/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Helper functions for text rendering using the ascii.png sprites.
  */

#include <thcrap.h>
#include "thcrap_tsa.h"
#include "ascii.hpp"

logger_t ascii_log("ASCII patching error");

class ascii_params_t {
#define PARAM(type, name, zerovalue) \
private: \
	type _##name = zerovalue; \
public: \
	type name() \
	{ \
		if(_##name == zerovalue) { \
			ascii_log.errorf("%s not given, crashing...", #name); \
		} \
		return _##name; \
	}

	PARAM(void *, ClassPtr, nullptr);
	PARAM(vector2_t *, Scale, nullptr);
	PARAM(float, CharWidth, 0.0f);

#undef PARAM

	int update(x86_reg_t *regs, json_t *bp_info)
	{
		auto l = ascii_log.prefixed("`ascii_params`: ");

		auto update_ptr = [&] (void **ptr, const char *var) {
			auto j = json_object_get(bp_info, var);
			if(j) {
				auto ret = json_immediate_value(j, regs);
				if(!ret) {
					l.errorf("%s is zero", var);
					return false;
				}
				*ptr = (void*)ret;
			}
			return true;
		};
		auto update_rel = [&] (void **ptr, const char *var) {
			auto ret = update_ptr(ptr, var);
			if(ret) {
				*(char**)ptr += (size_t)_ClassPtr;
			}
			return ret;
		};

#define UPDATE_REL(var) update_rel((void**)&_##var, "." #var)

		auto j = json_object_get(bp_info, "CharWidth");
		if(json_is_real(j)) {
			_CharWidth = (float)json_real_value(j);
		} else if(json_is_integer(j)) {
			_CharWidth = (float)json_integer_value(j);
		} else if(j) {
			l.errorf("\"CharWidth\" must be a JSON real or integer");
		}

		update_ptr(&_ClassPtr, "ClassPtr")
			&& UPDATE_REL(Scale);

#undef UPDATE_REL
		return 1;
	}

	template <typename F> void with_scale_x(float scale_temp, F func)
	{
		auto scale = Scale();
		auto prev = scale->x;
		scale->x = scale_temp;
		func();
		scale->x = prev;
	}
};

ascii_params_t params;

float ascii_char_width()
{
	return params.CharWidth() * params.Scale()->x;
}

float ascii_extent(const char *str)
{
	return strlen(str) * ascii_char_width();
}

float ascii_align_center(float x, const char *str)
{
	return x - (ascii_extent(str) / 2.0f);
}

float ascii_align_right(float x, const char *str)
{
	return x - ascii_extent(str);
}

// Note that we don't pass [pos] by reference. Copying it is more convenient,
// because ZUN often subtracts the expected width from [pos.x] before going to
// the next line...
int ascii_vpatchf_th06(
	ascii_put_func_t &putfunc, vector3_t pos, const char* fmt, va_list va
)
{
#define SCORE_LEN DECIMAL_DIGITS_BOUND(int)

	// We should only retrieve things here that are absolutely
	// needed for everything - don't want to crash unnecessarily.
	auto *ClassPtr = params.ClassPtr();

	auto put_10_digit_score_and_advance_x = [&] (
		vector3_t &pos, bool leading_zeroes, int score
	) {
		char score_buf[SCORE_LEN + 1];
		sprintf(score_buf, (leading_zeroes) ? "%.10d" : "%10d", score);
		params.with_scale_x(0.9f, [&] {
			putfunc(ClassPtr, pos, score_buf);
			pos.x += (10 * ascii_char_width());
		});
		return 0;
	};

	const stringref_t TH06_ASCII_PREFIX = "th06_ascii_";

	auto id = strings_id(fmt);
	if(!id || strncmp(id, TH06_ASCII_PREFIX.str, TH06_ASCII_PREFIX.len)) {
		auto single_str = strings_vsprintf((size_t)&pos, fmt, va);
		return putfunc(ClassPtr, pos, single_str);
	}
	id += TH06_ASCII_PREFIX.len;

	/// Specially rendered strings that ignore the original format
	/// ----------------------------------------------------------
	// Score format - make sure to scale the numbers to the same on-screen
	// width even if it hits 10 digits or more, as done by TH07.
	if(!strcmp(id, "score_format")) {
		// Compensate for high score and current score being rendered
		// using the same format string, since we want to use the same
		// scaling for both so that the numbers line up nicely.
		// Relies on the correct rendering order to work without
		// glitching the initial frame, though. (High(er) score first,
		// current score second.)
		static bool last_len_10 = false;
		auto score = va_arg(va, int);
		auto is_10_digit = (score >= 1000000000);
		if(is_10_digit) {
			last_len_10 = true;
			return put_10_digit_score_and_advance_x(pos, false, score);
		} else if(last_len_10) {
			last_len_10 = false;
			return put_10_digit_score_and_advance_x(pos, true, score);
		}
		char score_buf[SCORE_LEN + 1];
		sprintf(score_buf, fmt, score);
		return putfunc(ClassPtr, pos, score_buf);
	}
	// Practice format - reuse the translations for the in-game
	// stage string, right-align, and print the score separately
	if(!strcmp(id, "practice_format")) {
		const float SPLIT_POINT = pos.x + (ascii_char_width() * 7);
		auto stage_id = va_arg(va, int);
		auto score = va_arg(va, int);

		auto stage_fmt = strings_get_fallback({ "th06_ascii_centered_stage_format", "STAGE %d" });
		VLA(char, buf, stage_fmt.len + SCORE_LEN + 1);
		defer({ VLA_FREE(buf); });

		sprintf(buf, stage_fmt.str, stage_id);
		pos.x = ascii_align_right(SPLIT_POINT, buf);
		putfunc(ClassPtr, pos, buf);

		sprintf(buf, "%.10d", score);
		pos.x = SPLIT_POINT + (2 * ascii_char_width());
		return putfunc(ClassPtr, pos, buf);
	}
	// Result format - print name and the (bracketed stage) separately,
	// and scale the score to always fit into 9 digits
	const stringref_t ID_RESULT = "result_score_format";
	if(!strncmp(id, ID_RESULT.str, ID_RESULT.len)) {
		auto name = va_arg(va, const char*);
		auto score = va_arg(va, int);

		putfunc(ClassPtr, pos, name);
		pos.x += ((8 + 1) * ascii_char_width());

		put_10_digit_score_and_advance_x(pos, false, score);

		id += ID_RESULT.len;

		if(!strcmp(id, "_clear")) {
			auto str = strings_get_fallback({ "th06_ascii_result_clear", "(C)" });
			return putfunc(ClassPtr, pos, str.str);
		}
		int stage = 1;
		if(strcmp(id, "_1")) {
			stage = va_arg(va, int);
		}
		char suffix[4] = { '(', (stage % 10) + '0', ')', '\0' };
		return putfunc(ClassPtr, pos, suffix);
	}
	// Ranks in the Result screen after clearing the game - use the
	// regular translations rather than these right-aligned duplicates.
	const stringref_t ID_RESULT_RANK = "result_rank_";
	if(!strncmp(id, ID_RESULT_RANK.str, ID_RESULT_RANK.len)) {
		stringref_t rank = id + ID_RESULT_RANK.len;
		const char *fallback = fmt;
		while(*fallback == ' ') {
			fallback++;
		}

		VLA(char, regular_rank_id, TH06_ASCII_PREFIX.len + rank.len + 1);
		defer({ VLA_FREE(regular_rank_id); });

		auto p = regular_rank_id;
		p = stringref_copy_advance_dst(p, TH06_ASCII_PREFIX);
		p = stringref_copy_advance_dst(p, rank);

		auto str = strings_get_fallback({ regular_rank_id, fallback });
		pos.x = ascii_align_right(398.0f, str.str);
		return putfunc(ClassPtr, pos, str.str);
	}
	/// ----------------------------------------------------------

	auto single_str = strings_vsprintf((size_t)&pos, fmt, va);

	// Strings centered inside the playfield
	const auto PLAYFIELD_CENTER = 224.0f;
	const stringref_t ID_CENTERED = "centered";
	if(!strncmp(id, ID_CENTERED.str, ID_CENTERED.len)) {
		pos.x = ascii_align_center(PLAYFIELD_CENTER, single_str);
	} else if(!strcmp(id, "fullpower")) {
		auto center_cur = pos.x + (ascii_extent("Full Power Mode!!") / 2.0f);
		pos.x = ascii_align_center(center_cur, single_str);
	} else if(!strcmp(id, "bonus_format")) {
		auto source_w = ascii_extent("BONUS 12345678");
		auto source_w_half = source_w / 2.0f;
		auto shift = PLAYFIELD_CENTER - source_w_half - 104.0f;
		auto center_cur = pos.x + source_w_half + shift;
		pos.x = ascii_align_center(center_cur, single_str);
	}

	return putfunc(ClassPtr, pos, single_str);

#undef SCORE_LEN
}

int ascii_vpatchf_th165(
	ascii_put_func_t &putfunc, vector3_t pos, const char* fmt, va_list va
)
{
	auto single_str = strings_vsprintf((size_t)&pos, fmt, va);
	const stringref_t REPLAY_SAVE_PREFIX = "th165_ascii_replay_save";

	// Center the replay save menu inside the playfield
	auto id = strings_id(fmt);
	if(id && !strncmp(id, REPLAY_SAVE_PREFIX.str, REPLAY_SAVE_PREFIX.len)) {
		const auto PLAYFIELD_CENTER = 320.0f;
		pos.x = ascii_align_center(PLAYFIELD_CENTER, single_str);
	}
	return putfunc(params.ClassPtr(), pos, single_str);
}

extern "C" __declspec(dllexport) int __stdcall ascii_vpatchf(
	ascii_put_func_t &putfunc, vector3_t &pos, const char* fmt, va_list va
)
{
	if(game_id == TH06) {
		return ascii_vpatchf_th06(putfunc, pos, fmt, va);
	} else if(game_id == TH165) {
		return ascii_vpatchf_th165(putfunc, pos, fmt, va);
	}
	auto single_str = strings_vsprintf((size_t)&pos, fmt, va);
	return putfunc(params.ClassPtr(), pos, single_str);
}

extern "C" __declspec(dllexport) int BP_ascii_params(x86_reg_t *regs, json_t *bp_info)
{
	return params.update(regs, bp_info);
}

void ascii_repatch()
{
	auto stringdefs = jsondata_get("stringdefs.js");
	auto strings_set = [&stringdefs] (const char *id, const std::string& str) {
		json_object_set_new(stringdefs, id, json_stringn(str.c_str(), str.size()));
	};
	auto right_pad = [] (const stringref_t& strref, int padchars) {
		auto str = std::string(strref.str, strref.len);
		if(padchars <= 0) {
			return str;
		}
		return str + std::string(padchars, ' ');
	};

	// TH165: Recreate the Replay format strings
	if(game_id == TH165) {
		size_t day_width_max = 0;
		const string_named_t TH165_DAYS[] = {
			{ "th165_ascii_replay_sun", "Sun" },
			{ "th165_ascii_replay_mon", "Mon" },
			{ "th165_ascii_replay_tue", "Tue" },
			{ "th165_ascii_replay_wed", "Wed" },
			{ "th165_ascii_replay_thu", "Thu" },
			{ "th165_ascii_replay_fri", "Fri" },
			{ "th165_ascii_replay_sat", "Sat" },
			{ "th165_ascii_replay_sun2", "2nd Sun" },
			{ "th165_ascii_replay_mon2", "2nd Mon" },
			{ "th165_ascii_replay_tue2", "2nd Tue" },
			{ "th165_ascii_replay_wed2", "2nd Wed" },
			{ "th165_ascii_replay_thu2", "2nd Thu" },
			{ "th165_ascii_replay_fri2", "2nd Fri" },
			{ "th165_ascii_replay_sat2", "2nd Sat" },
			{ "th165_ascii_replay_sun3", "3rd Sun" },
			{ "th165_ascii_replay_mon3", "3rd Mon" },
			{ "th165_ascii_replay_tue3", "3rd Tue" },
			{ "th165_ascii_replay_wed3", "3rd Wed" },
			{ "th165_ascii_replay_thu3", "3rd Thu" },
			{ "th165_ascii_replay_fri3", "3rd Fri" },
			{ "th165_ascii_replay_sat3", "3rd Sat" },
			{ "th165_ascii_replay_diary", "Diary" },
		};
		for(auto &day : TH165_DAYS) {
			auto str = strings_get_fallback(day);
			day_width_max = max(day_width_max, str.len);
		}
		const int USERNAME_LEN = 4;
		auto number = strings_get_fallback({ "th06_ascii_2_digit_number_format", "No.%.2d" });
		auto user = strings_get_fallback({ "th06_ascii_replay_user", "User" });
		size_t number_printed_len = _scprintf(number.str, 25);
		// The save menu shouldn't be made larger depending on the User translation
		auto replay_col1_len = max(max(number_printed_len, user.len), USERNAME_LEN);
		auto number_save = std::string(number.str, number.len);
		auto number_padded = right_pad(number, replay_col1_len - number_printed_len);
		auto user_padded = right_pad(user, replay_col1_len - user.len);
		auto username_padded = right_pad("%s", replay_col1_len - USERNAME_LEN);

		const std::string NAME_AND_DATE_EMPTY = " ------------ --/--/-- ";
		const std::string NAME_AND_DATE = " %s %.2d/%.2d/%.2d ";

		// Empty format
		auto fmt = number_save + NAME_AND_DATE_EMPTY;
		fmt += std::string(day_width_max + 2, '-');
		strings_set("th165_ascii_replay_save_empty", fmt);

		fmt.replace(0, number_save.size(), number_padded);
		fmt.insert(number_padded.size() + NAME_AND_DATE_EMPTY.size(), "--:--  ");
		fmt += "  ---%%";
		strings_set("th165_ascii_replay_number_empty", fmt);

		fmt.replace(0, number_padded.size(), user_padded);
		strings_set("th165_ascii_replay_user_empty", fmt);

		// Regular format
		fmt = number_save + NAME_AND_DATE;
		fmt += "%" + std::to_string(day_width_max) + "s-%d";
		strings_set("th165_ascii_replay_save", fmt);

		fmt.replace(0, number_save.size(), number_padded);
		fmt.insert(number_padded.size() + NAME_AND_DATE.size(), "%.2d:%.2d  ");
		// 2-digit slowdown percentages aren't aligned correctly in the
		// original game, actually.
		fmt += " %4.1f%%";
		strings_set("th165_ascii_replay_number", fmt);

		fmt.replace(0, number_padded.size(), username_padded);
		strings_set("th165_ascii_replay_user", fmt);
	}
}

extern "C" __declspec(dllexport) void ascii_mod_repatch(json_t *files_changed)
{
	const char *fn;
	json_t *val;
	json_object_foreach(files_changed, fn, val) {
		if(strstr(fn, "stringdefs.")) {
			ascii_repatch();
		}
	}
}

extern "C" __declspec(dllexport) void ascii_mod_init(json_t *files_changed)
{
	ascii_repatch();
}
