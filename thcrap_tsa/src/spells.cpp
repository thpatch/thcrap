/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Spell card patching via breakpoints.
  */

#include <thcrap.h>
// (TL notes should be removed before every spell card declaration)
#include <tlnote.hpp>
#include "thcrap_tsa.h"

// Lookup cache
static std::vector<std::string> cache_array_spell_ids;

static std::string spell_id_params_to_string(std::vector<patch_val_t>& spell_id_params, const char* sep) {
	std::string str_spell_id;
	for (size_t i = 0; i < spell_id_params.size(); i++) {
		char a_num[DECIMAL_DIGITS_BOUND(uint64_t) + 1] = {};
		const char* append = a_num;

		switch (spell_id_params[i].type) {
			case PVT_STRING:
				append = spell_id_params[i].str.ptr;
				break;
			case PVT_SBYTE:	case PVT_SWORD:	case PVT_SDWORD: case PVT_SQWORD:
				_i64toa(spell_id_params[i].sq, a_num, 10);
				break;
			case PVT_BYTE: case PVT_WORD: case PVT_DWORD: case PVT_QWORD:
				_ui64toa(spell_id_params[i].q, a_num, 10);
				break;
		}
		str_spell_id.append(append);

		if ((i + 1) != spell_id_params.size()) {
			str_spell_id.append(sep);
		}
	}
	return str_spell_id;
}

size_t BP_spell_id(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	json_t *spell_id = json_object_get(bp_info, "spell_id");
	json_t *spell_id_type = json_object_get(bp_info, "spell_id_type");
	json_t *spell_id_real = json_object_get(bp_info, "spell_id_real");
	json_t *spell_rank = json_object_get(bp_info, "spell_rank");
	// ----------
	// These have to be signed...
	int int_spell_id = 0;
	int int_spell_id_real = 0;

	if (json_is_array(spell_id)) {
		std::vector<patch_val_t> spell_id_params;

		const char* sep = json_object_get_string(bp_info, "separator");
		if (!sep) sep = "+";

		size_t count_down_idx = -1;

		json_t* val;
		json_array_foreach_scoped(size_t, i, spell_id, val) {
			auto it = json_object_get_typed(val, regs, "param", patch_parse_type(json_object_get_string(val, "type")));
			spell_id_params.push_back(it);

			bool count_down = json_object_get_eval_bool_default(val, "count_down", false, JEVAL_DEFAULT);

			if (count_down) {
				if (count_down_idx != -1) {
					log_print("WARNING: count_down in spell_id can only be set for one parameter\n");
				}
				if (it.type == PVT_STRING) {
					log_print("WARNING: count_down in spell_id cannot be true for string parameters\n");
				}
				else {
					count_down_idx = i;
				}
			}
		}

		cache_array_spell_ids = {};
		if (count_down_idx == -1) {
			cache_array_spell_ids.push_back(spell_id_params_to_string(spell_id_params, sep));
		}
		else {
			for (; spell_id_params[count_down_idx].q; spell_id_params[count_down_idx].q--) {
				cache_array_spell_ids.push_back(spell_id_params_to_string(spell_id_params, sep));
			}
			cache_array_spell_ids.push_back(spell_id_params_to_string(spell_id_params, sep));
		}
	}
	else if (patch_value_type_t type = patch_parse_type(json_string_value(spell_id_type)); type == PVT_STRING) {
		auto val = json_typed_value(spell_id, regs, type);
		if (val.type == PVT_STRING && val.str.ptr) {
			cache_array_spell_ids = { val.str.ptr };
		}
		else {
			// spell_id_real and spell_rank could still be set with an int value
			goto spell_id_int;
		}
	}
	else {
		if (spell_id || spell_id_real || spell_rank) {
			if (spell_id) {
				int_spell_id = json_immediate_value(spell_id, regs);
				int_spell_id_real = int_spell_id;
			}
		spell_id_int:
			if (spell_id_real) {
				int_spell_id_real = json_immediate_value(spell_id_real, regs);
			}
			if (spell_rank) {
				size_t rank = json_immediate_value(spell_rank, regs);
				int_spell_id = int_spell_id_real - rank;
			}

			cache_array_spell_ids = {};

			// ...otherwise this loop goes on forever if the spell ID is 0
			while (int_spell_id_real >= int_spell_id) {
				char str_spell_id[16] = {};
				itoa(int_spell_id_real, str_spell_id, 10);
				cache_array_spell_ids.push_back(str_spell_id);
				int_spell_id_real--;
			}
		}
	}
	return 1;
}

size_t BP_spell_name(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	const char **spell_name = (const char**)json_object_get_pointer(bp_info, regs, "spell_name");
	// ----------

	// Other breakpoints
	// -----------------
	BP_spell_id(regs, bp_info);
	// -----------------

	tlnote_remove();

	if(cache_array_spell_ids.size()) {
		const char *new_name = NULL;
		json_t *spells = jsondata_game_get("spells.js");

		// Count down from the real number to the given number
		// until we find something
		for (auto& i : cache_array_spell_ids) {
			if (new_name = json_object_get_string(spells, i.c_str())) {
				*spell_name = new_name;
				return breakpoint_cave_exec_flag(bp_info);
			}
		}
	}
	return 1;
}

size_t BP_spell_comment_line(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	const char **str = (const char**)json_object_get_register(bp_info, regs, "str");
	size_t comment_num = json_object_get_hex(bp_info, "comment_num");
	size_t line_num = json_object_get_hex(bp_info, "line_num");
	// ----------

	// Other breakpoints
	// -----------------
	BP_spell_id(regs, bp_info);
	// -----------------

	if(str && comment_num) {
		json_t *json_cmt = NULL;
		json_t *spellcomments = jsondata_game_get("spellcomments.js");

		size_t cmt_key_str_len = strlen("comment_") + 16 + 1;
		VLA(char, cmt_key_str, cmt_key_str_len);
		sprintf(cmt_key_str, "comment_%zu", comment_num);

		// Count down from the real number to the given number
		// until we find something

		for (auto& i : cache_array_spell_ids) {
			json_cmt = json_object_get(spellcomments, i.c_str());
			json_cmt = json_object_get(json_cmt, cmt_key_str);
			if (json_is_array(json_cmt)) {
				*str = json_array_get_string_safe(json_cmt, line_num);
				VLA_FREE(cmt_key_str);
				return breakpoint_cave_exec_flag(bp_info);
			}
		}
		VLA_FREE(cmt_key_str);
	}
	return 1;
}

size_t BP_spell_owner(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	const char **spell_owner = (const char**)json_object_get_register(bp_info, regs, "spell_owner");
	// ----------

	// Other breakpoints
	// -----------------
	BP_spell_id(regs, bp_info);
	// -----------------

	if (spell_owner && cache_array_spell_ids.size()) {
		const char *new_owner = NULL;
		json_t *spellcomments = jsondata_game_get("spellcomments.js");

		for (auto& i : cache_array_spell_ids) {
			if (const char* new_owner = json_object_get_string(json_object_get(spellcomments, i.c_str()), "owner")) {
				*spell_owner = new_owner;
				return breakpoint_cave_exec_flag(bp_info);
			}
		}
	}
	return 1;
}


// Legacy TH185 specific spell breakpoints

char th185_spell_id[8] = {};

size_t BP_th185_spell_id(x86_reg_t* regs, json_t* bp_info) {
	*th185_spell_id = 0;
	const char* sub_name = (char*)json_object_get_immediate(bp_info, regs, "sub_name");
	if (strcmp(sub_name, "Boss01tBossCard1") == 0) {
		strcpy(th185_spell_id, "0_1");
		return breakpoint_cave_exec_flag(bp_info);
	}
	if (strlen(sub_name) != 15
		|| strncmp(sub_name, "Boss", 4)
		|| strncmp(sub_name + 6, "BossCard", 8)
		|| sub_name[4] > '9' || sub_name[4] < '0'
		|| sub_name[5] > '9' || sub_name[5] < '0'
		|| sub_name[14] > '9' || sub_name[14] < '0') {
		return breakpoint_cave_exec_flag(bp_info);
	}
	char* p = th185_spell_id;
	if (sub_name[4] > '0')
		*p++ = sub_name[4];
	*p++ = sub_name[5];
	*p++ = '_';
	*p++ = sub_name[14];
	return breakpoint_cave_exec_flag(bp_info);
}

size_t BP_th185_spell_name(x86_reg_t* regs, json_t* bp_info) {
	if (*th185_spell_id) {
		json_t* spell_name = json_object_get(jsondata_game_get("spells.js"), th185_spell_id);
		if (json_is_string(spell_name))
			*json_object_get_pointer(bp_info, regs, "spell_name") = (size_t)json_string_value(spell_name);
		*th185_spell_id = 0;
	}
	return breakpoint_cave_exec_flag(bp_info);
}

void spells_mod_init(void)
{
	jsondata_game_add("spells.js");
	jsondata_game_add("spellcomments.js");
}
