/**
* Touhou Community Reliant Automatic Patcher
* Team Shanghai Alice support plugin
*
* ----
*
* Breakpoints for mission discriptions in TH095 and TH125
*/

#include <thcrap.h>
#include <bp_file.h>
#include "thcrap_tsa.h"
#include "layout.h"

/* pseudocode for how th125 calls the breakpoints:
 *
 * for(line=0; line<3; line++) {
 *     str = decrypt_mission(line, ...); // BP_mission
 *     screenprintf(0, str, ...); // BP_mission_printf_hook (ignored)
 *     if( xoffset[line+3] >= 0 ) { // BP_mission_check_furi_a
 *         str = decrypt_mission(line + 3, ...); // BP_mission
 *         screenprintf(xoffset[line+3], str, ...); // BP_mission_printf_hook
 *     }
 * }
 */

static int32_t mission_furi_a = -2;
static size_t laststage, lastscene, lastchara;

int BP_mission(x86_reg_t *regs, json_t *bp_info)
{
	// This breakpoint is supposed to replace mission string decryption proc in th95 and th125.
	// For th095, the function is stdcall, and takes (char* text, int line, int stage, int scene) as arguments
	// For th125, the function is stdcall (int stage, int scene, int chara), with additional arguments in registers: int eax=line, char *ecx=text.
	// Both return const char* to a static string, a la asctime()

	// Parameters
	// ----------
	size_t *line_reg = json_object_get_register(bp_info, regs, "line");
	size_t line = line_reg ? *line_reg : *(size_t*)(regs->esp + 4 + json_object_get_hex(bp_info, "line"));
	size_t stage = *(size_t*)(regs->esp + 4 + json_object_get_hex(bp_info, "stage"));
	size_t scene = *(size_t*)(regs->esp + 4 + json_object_get_hex(bp_info, "scene"));
	size_t chara_offset = json_object_get_hex(bp_info, "chara");
	size_t chara = chara_offset ? *(size_t*)(regs->esp + 4 + chara_offset) : 0;
	// ----------

	if (line < 3) {
		// save those for BP_mission_check_furi_a
		laststage = stage;
		lastscene = scene;
		lastchara = chara;

		// disable furigana hook for non-furigana lines
		mission_furi_a = -2;
	}

	json_t *missions = jsondata_game_get("missions.js");
	char mission_key_str[16*3 + 2 + 1];
	sprintf(mission_key_str, "%u_%u_%u", chara, stage, scene);
	json_t *mission = json_object_get(missions, mission_key_str);

	if (!mission) {
		return 1;
	}

	// return the lines
	json_t *lines = json_object_get(mission, "lines");
	if (!lines || line >= json_array_size(lines)) {
		regs->eax = (size_t)" ";
	}
	else{
		regs->eax = (size_t)json_array_get_string(lines, line);
	}

	// return one function higher
	regs->retaddr = ((size_t*)regs->esp)[1];
	return 0;
}

int BP_mission_check_furi_a(x86_reg_t *regs, json_t *bp_info)
{
	// This breakpoint is supposed to replace code which compares furi_a to zero (i.e. cmp [blah], 0)
	// The comparision is done for jl, so only CF and SF need to be changed

	// We also use this oppertunity to prepare furi_a for BP_mission_printf_hook

	// Parameters
	// ----------
	size_t line = 3+*json_object_get_register(bp_info, regs, "line");
	// ----------

	// prepare furi_a
	json_t *missions = jsondata_game_get("missions.js");
	char mission_key_str[16 * 3 + 2 + 1];
	sprintf(mission_key_str, "%u_%u_%u", lastchara, laststage, lastscene);
	json_t *mission = json_object_get(missions, mission_key_str);
	if (!mission) {
		mission_furi_a = -2;
		return 1; // original string
	}
	json_t *furi = json_object_get(mission, "furi");
	if (!furi || line - 3 >= json_array_size(furi)) {
		mission_furi_a = -1;
	}
	else {
		json_t* furiline = json_array_get(furi, line - 3);
		if (json_is_integer(furiline)) {
			mission_furi_a = (int32_t)json_integer_value(furiline);
		}
		else {
			assert(json_is_array(furiline));
			assert(json_array_size(furiline) == 2);
			const char* begin = json_array_get_string(furiline, 0);
			const char* target = json_array_get_string(furiline, 1);
			const char* str = " ";
			json_t *lines = json_object_get(mission, "lines");
			if (lines && line < json_array_size(lines)) {
				str = json_array_get_string(lines, line);
			}

			auto font_bottom = font_block_get(0).unwrap_or(nullptr);
			auto font_ruby = font_block_get(2).unwrap_or(nullptr);

			mission_furi_a = ruby_offset_half(begin, target, str, font_bottom, font_ruby);
		}
	}

	// set flags
	regs->flags &= ~(1|128); // unset CF|SF
	if (mission_furi_a  < 0) {
		regs->flags |= 128; // set SF
	}
	return 0;
}

int BP_mission_printf_hook(x86_reg_t *regs, json_t *bp_info)
{
	// This breakpoint is supposed to hook the beggining of a printf-like function in th125 (45F8B0).
	// esi is an object containing some of the rendering parameters (e.g. whether to draw outline)
	// edi is furi_b (horizontal stretching of the string. it looks like it splits strings into 2-byte chunks and spreads them)
	// ebx is TSA font block number (0 for normal text, 2 for furigana)
	// argument 1 is stroke color (0RGB)
	// argument 2 is outline color (0RGB)
	// argument 3 furi_a. horizontal offset for furigana
	// argument 4 is format string
	// arguments 5+ are printf arguments

	if (mission_furi_a < -1) return 1; // don't change if -2

	*(int32_t*)(regs->esp + 4 + 12) = mission_furi_a;
	regs->edi = 0; // disable stetching, because it looks ugly on non-japanese strings
	mission_furi_a = -2; // reset

	return 1;
}

void missions_mod_init(void)
{
	jsondata_game_add("missions.js");
}
