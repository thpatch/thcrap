#include <thcrap.h>
#include <jansson_ex.h>

extern "C" int BP_dict_translate(x86_reg_t* regs, json_t* bp_info) {
	const char** str = (const char**)json_object_get_pointer(bp_info, regs, "str");
	if (!str || !*str) {
		return 1;
	}
	const char* str_u8 = EnsureUTF8(*str, strlen(*str));

	auto func_end = [&bp_info](const char* str) {
		if (json_object_get_eval_bool_default(bp_info, "dump", false, JEVAL_DEFAULT))
			log_printf("(Dictionary) Dumped string: %s\n", str);

		free((void*)str);
	};
	defer(func_end(str_u8));

	const char* dictdefs_fn = json_object_get_string(bp_info, "dictdefs");
	const char* dicttrans_fn = json_object_get_string(bp_info, "dicttrans");

	if (!dictdefs_fn)
		dictdefs_fn = "dictdefs.js";
	if (!dicttrans_fn)
		dicttrans_fn = "dicttrans.js";

	json_t* dictdefs = jsondata_game_get(dictdefs_fn);
	json_t* dicttrans = jsondata_game_get(dicttrans_fn);

	if (!dictdefs || !dicttrans)
		return 1;	

	const char* trans_id = json_object_get_string(dictdefs, str_u8);
	const char* translation = json_object_get_string(dicttrans, trans_id);

	if (translation) {
		*str = translation;
	}

	return 1;
}


extern "C" {

TH_EXPORT void dict_mod_init() {
	jsondata_game_add("dictdefs.js");
	jsondata_game_add("dicttrans.js");

	json_t* breakpoints = json_object_get(runconfig_json_get(), "breakpoints");

	json_t* bp_info;
	const char* bp_name;
	json_object_foreach_fast(breakpoints, bp_name, bp_info) {
		if (!strncmp(bp_name, "dict_trans", strlen("dict_trans"))) {
			if (const char* dictdefs_fn = json_object_get_string(bp_info, "dictdefs")) {
				jsondata_game_add(dictdefs_fn);
			}
			if (const char* dicttrans_fn = json_object_get_string(bp_info, "dicttrans")) {
				jsondata_game_add(dicttrans_fn);
			}
		}
	}	
}

}
