/**
* Touhou Community Reliant Automatic Patcher
* Team Shanghai Alice support plugin
*
* ----
*
* Breakpoint for mission discriptions in TH095 and TH125
*/

#include <thcrap.h>
#include <bp_file.h>
#include "thcrap_tsa.h"

int BP_mission(x86_reg_t *regs, json_t *bp_info)
{
	// This breakpoint is supposed to replace mission string decryption proc in th95 and th125.
	// For th095, the function is stdcall, and takes (char* text, int line, int stage, int scene) as arguments
	// For th125, the function is stdcall (inst stage, int scene, int chara), with additional arguments in registers: int eax=line, char *ecx=text.
	// Both return const char* to a static string, a la asctime()

	// Parameters
	// ----------
	size_t *line_reg = json_object_get_register(bp_info, regs, "line");
	size_t line = line_reg ? *line_reg : *(size_t*)(regs->esp + 4 + json_object_get_hex(bp_info, "line"));
	size_t stage = *(size_t*)(regs->esp + 4 + json_object_get_hex(bp_info, "stage"));
	size_t scene = *(size_t*)(regs->esp + 4 + json_object_get_hex(bp_info, "scene"));
	size_t chara_offset = json_object_get_hex(bp_info, "chara");
	size_t chara = chara_offset ? *(size_t*)(regs->esp + 4 + chara_offset) : 0;

	size_t ret_fixup = json_object_get_hex(bp_info, "ret_fixup");
	// ----------

	json_t *missions = jsondata_game_get("missions.js");
	VLA(char, mission_key_str, 16*3 + 2 + 1);
	sprintf(mission_key_str, "%u_%u_%u", chara, stage, scene);
	json_t *mission = json_object_get(missions, mission_key_str);
	if (!mission) {
		return 1;
	}

	if (line > json_array_size(mission)) {
		regs->eax = (size_t)"";
	}
	else{
		regs->eax = (size_t)json_array_get_string(mission, line);
	}

	// return one function higher
	regs->retaddr = ((size_t*)regs->esp)[1];
	regs->esp += 4 + ret_fixup; // ret_fixup is needed because stdcall
	return 0;
}

void missions_mod_init(void)
{
	jsondata_game_add("missions.js");
}