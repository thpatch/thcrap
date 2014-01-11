/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * On-the-fly th06msg patcher (in-game dialog format *since* th06)
  *
  * Portions adapted from xarnonymous' Touhou Toolkit
  * http://code.google.com/p/thtk/
  */

#include <thcrap.h>
#include "thcrap_tsa.h"

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
	WORD time;
	BYTE type;
	BYTE length;
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
	BYTE data[];
} PACK_ATTRIBUTE th06_msg_t;

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
	WORD side;
	WORD linenum;
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
	BYTE str[];
} PACK_ATTRIBUTE hard_line_data_t;

// Supported opcode commands
typedef enum {
	OP_UNKNOWN = 0,
	OP_HARD_LINE,
	OP_AUTO_LINE,
	OP_AUTO_END,
	OP_DELETE
} op_cmd_t;

typedef struct {
	BYTE op;
	op_cmd_t cmd;
	const char *type;
} op_info_t;

typedef struct {
	// Format information
	op_info_t* op_info;
	size_t op_info_num;

	// Encryption
	BYTE* enc_vars;
	size_t enc_var_count;
	EncryptionFunc_t enc_func;

	// JSON objects in the diff file
	const json_t *diff_entry;
	const json_t *diff_code;
	const json_t *diff_lines;

	// Current input / output
	th06_msg_t* cmd_in;
	th06_msg_t* cmd_out;
	int entry, time, ind;

	// Last state
	const th06_msg_t *last_line_cmd;
	const op_info_t *last_line_op;

	// Indices
	size_t cur_line;
} patch_msg_state_t;

/**
  * Line replacement function type.
  *
  * Parameters
  * ----------
  *	th06_msg_t *cmd_out
  *		Target .msg command
  *
  *	patch_msg_state_t *state
  *		Patch state object
  *
  *  const char *rep
  *		Replacement string
  *
  * Returns nothing.
  */
typedef void (*ReplaceFunc_t)(th06_msg_t *cmd_out, patch_msg_state_t *state, const char *new_line);

const op_info_t* get_op_info(patch_msg_state_t* state, BYTE op)
{
	if(state && state->op_info && state->op_info_num) {
		size_t i;
		for(i = 0; i < state->op_info_num; i++) {
			if(state->op_info[i].op == op) {
				return &state->op_info[i];
			}
		}
	}
	return NULL;
}

// Because string comparisons in the main loop are not nice
op_cmd_t op_str_to_cmd(const char *str)
{
	if(!str) {
		return OP_UNKNOWN;
	}
	if(!strcmp(str, "hard_line")) {
		return OP_HARD_LINE;
	}
	if(!strcmp(str, "auto_line")) {
		return OP_AUTO_LINE;
	}
	if(!strcmp(str, "auto_end")) {
		return OP_AUTO_END;
	}
	if(!strcmp(str, "delete")) {
		return OP_DELETE;
	}
	return OP_UNKNOWN;
}

int patch_msg_state_init(patch_msg_state_t *state, json_t *format)
{
	json_t *encryption = json_object_get(format, "encryption");
	json_t *opcodes = json_object_get(format, "opcodes");
	json_t *op_obj;
	const char *key;
	int i = 0;

	if(!json_is_object(opcodes)) {
		// Nothing to do with no definitions...
		return -1;
	}

	ZeroMemory(state, sizeof(patch_msg_state_t));

	// Prepare opcode info
	state->op_info_num = json_object_size(opcodes);
	state->op_info = (op_info_t*)malloc(sizeof(op_info_t) * state->op_info_num);
	json_object_foreach(opcodes, key, op_obj) {
		const char *cmd_str;
		state->op_info[i].op = atoi(key);
		state->op_info[i].type = json_object_get_string(op_obj, "type");

		cmd_str = json_object_get_string(op_obj, "cmd");
		state->op_info[i].cmd = op_str_to_cmd(cmd_str);
		i++;
	}

	// Prepare encryption vars
	if(json_is_object(encryption)) {
		const char *encryption_func = json_object_get_string(encryption, "func");
		json_t *encryption_vars = json_object_get(encryption, "vars");

		// Resolve function
		if(encryption_func) {
			state->enc_func = (EncryptionFunc_t)runconfig_func_get(encryption_func);
		}
		if(json_is_array(encryption_vars)) {
			size_t i;

			state->enc_var_count = json_array_size(encryption_vars);

			state->enc_vars = (BYTE*)malloc(sizeof(BYTE) * state->enc_var_count);
			for(i = 0; i < state->enc_var_count; i++) {
				state->enc_vars[i] = (BYTE)json_array_get_hex(encryption_vars, i);
			}
		}
	}
	return 0;
}

void patch_msg_state_clear(patch_msg_state_t* state)
{
	SAFE_FREE(state->enc_vars);
	SAFE_FREE(state->op_info);
	ZeroMemory(state, sizeof(patch_msg_state_t));
}

size_t th06_msg_full_len(const th06_msg_t* msg)
{
	if(!msg) {
		return 0;
	}
	return (sizeof(th06_msg_t) + msg->length);
}

void replace_line(BYTE *dst, const char *rep, const size_t len, patch_msg_state_t *state)
{
	memcpy(dst, rep, len);
	if(state->enc_func) {
		state->enc_func(dst, len, state->enc_vars, state->enc_var_count);
	}
	state->cur_line++;
}

void replace_auto_line(th06_msg_t *cmd_out, patch_msg_state_t *state, const char *rep)
{
	cmd_out->length = strlen(rep) + 1;
	replace_line(cmd_out->data, rep, cmd_out->length, state);
}

void replace_hard_line(th06_msg_t *cmd_out, patch_msg_state_t *state, const char *rep)
{
	hard_line_data_t* line = (hard_line_data_t*)cmd_out->data;

	line->linenum = state->cur_line;
	cmd_out->length = strlen(rep) + 4 + 1;
	replace_line(line->str, rep, cmd_out->length - 4, state);
}

void format_slot_key(char *key_str, int time, const char *msg_type, int time_ind)
{
	if(msg_type) {
		sprintf(key_str, "%d_%s_%d", time, msg_type, time_ind);
	} else {
		sprintf(key_str, "%d_%d", time, time_ind);
	}
}

// Returns 1 if the output buffer should advance, 0 if it shouldn't.
int process_line(th06_msg_t *cmd_out, patch_msg_state_t *state, ReplaceFunc_t rep_func)
{
	const op_info_t* cur_op = get_op_info(state, cmd_out->type);

	// If we don't have a diff_code pointer, this is the first line of a new box.
	if(cur_op && !json_is_object(state->diff_code)) {
		size_t key_str_len = strlen(cur_op->type) + 32;
		VLA(char, key_str, key_str_len);
		state->ind++;
		format_slot_key(key_str, state->time, cur_op->type, state->ind);
		state->diff_code = json_object_get(state->diff_entry, key_str);
		state->diff_lines = json_object_get(state->diff_code, "lines");
		VLA_FREE(key_str);
	}

	if(json_is_array(state->diff_lines)) {
		const char *json_line = json_array_get_string(state->diff_lines, state->cur_line);
		if(json_line) {
			rep_func(cmd_out, state, json_line);
			state->last_line_cmd = cmd_out;
			state->last_line_op = cur_op;
			return 1;
		}
		// Line not present in the patch file, remove it
		return 0;
	}
	// If this dialog box contains no lines to patch, take original line
	return 1;
}

void box_end(patch_msg_state_t *state)
{
	if(!state->last_line_op) {
		return;
	}

	// Do we have any extra lines in the patch file?
	while(state->cur_line < json_array_size(state->diff_lines)) {
		const char *json_line = json_array_get_string(state->diff_lines, state->cur_line);
		if(json_line) {
			th06_msg_t *new_line_cmd;
			ptrdiff_t move_len;
			int hard_line;
			size_t line_offset;

			new_line_cmd = (th06_msg_t*)((BYTE*)state->last_line_cmd + th06_msg_full_len(state->last_line_cmd));
			move_len = (BYTE*)state->cmd_out + th06_msg_full_len(state->cmd_out) - (BYTE*)new_line_cmd;

			hard_line = (state->last_line_op->cmd == OP_HARD_LINE);
			line_offset = sizeof(th06_msg_t) + (hard_line ? 4 : 0);

			// Make room for the new line
			memmove(
				(BYTE*)(new_line_cmd) + line_offset + strlen(json_line) + 1,
				new_line_cmd,
				move_len
			);
			memcpy(new_line_cmd, state->last_line_cmd, line_offset);
			process_line(new_line_cmd, state, hard_line ? replace_hard_line : replace_auto_line);
			state->cmd_out += th06_msg_full_len(new_line_cmd);
		}
	}
	state->diff_code = NULL;
	state->cur_line = 0;
	state->diff_lines = NULL;
}

// Returns whether to advance the output buffer (1) or not (0)
int process_op(const op_info_t *cur_op, patch_msg_state_t* state)
{
	hard_line_data_t* line;
	WORD linenum;

	switch(cur_op->cmd) {
		case OP_DELETE:
			// Overwrite this command with the next one
			return 0;

		case OP_AUTO_LINE:
			if( (state->last_line_op) && (state->last_line_op->cmd == OP_HARD_LINE) ) {
				box_end(state);
			}
			return process_line(state->cmd_out, state, replace_auto_line);

		case OP_HARD_LINE:
			line = (hard_line_data_t*)state->cmd_out->data;
			// needs to be saved locally because box_end trashes the pointer
			linenum = line->linenum;

			if(linenum == 0) {
				// Line 0 always terminates a previous box
				box_end(state);
				if(state->last_line_op) {
					// Reset index when last_line_op->type differs from cur_op->type.
					// Needed e.g. for th07 stage 3
					const char *last_op_type = state->last_line_op->type;
					if(
						(!last_op_type && (last_op_type != cur_op->type)) 	||
						(last_op_type && !cur_op->type) ||
						(last_op_type && cur_op->type && !strcmp(last_op_type, cur_op->type))
					) {
						state->ind = -1;
					}
				}
			}
			state->cur_line = linenum;
			return process_line(state->cmd_out, state, replace_hard_line);

		case OP_AUTO_END:
			if(!state->last_line_op || state->last_line_op->cmd == OP_AUTO_LINE) {
				box_end(state);
			}
			return 1;
	}
	return 1;
}

int patch_msg(BYTE *file_inout, size_t size_out, size_t size_in, json_t *patch, json_t *format)
{
	DWORD* msg_out = (DWORD*)file_inout;
	DWORD* msg_in;
	int entry_new = 1;

	// Header data
	DWORD entry_offset_mul = json_object_get_hex(format, "entry_offset_mul");
	size_t entry_count;
	DWORD *entry_offsets_out;
	DWORD *entry_offsets_in;
	DWORD entry_offset_size;

	patch_msg_state_t state;

	if(!msg_out || !patch || !format || !entry_offset_mul) {
		return 1;
	}

	// Read format info
	if(patch_msg_state_init(&state, format)) {
		return 1;
	}

	// Make a copy of the input buffer
	msg_in = (DWORD*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size_in);
	memcpy(msg_in, msg_out, size_in);

	/**
	  * Not only is this unnecessary because we don't read from the output buffer,
	  * and the game stops reading at the last entry's 0 opcode anyway,
	  * it also causes a delayed crash in th09.
	  */
	// ZeroMemory(msg_out, size_out);

#ifdef _DEBUG
	{
		FILE* out = fopen("in.msg", "wb");
		if(out) {
			fwrite(msg_in, size_in, 1, out);
			fclose(out);
		}
	}
#endif

	// Read .msg header
	entry_count = *msg_in;
	entry_offsets_in = msg_in + 1;

	entry_offset_size = entry_count * entry_offset_mul;

	state.cmd_in = (th06_msg_t*)(msg_in + 1 + entry_offset_size);

	*msg_out = entry_count;
	entry_offsets_out = msg_out + 1;

	// Include whatever junk there might be in the header
	memcpy(entry_offsets_out, entry_offsets_in, entry_offset_size);

	state.cmd_out = (th06_msg_t*)(msg_out + 1 + entry_offset_size);

	for(;;) {
		const ptrdiff_t offset_in  = (BYTE*)state.cmd_in - (BYTE*)msg_in;
		const ptrdiff_t offset_out = (BYTE*)state.cmd_out - (BYTE*)msg_out;
		int advance_out = 1;
		const op_info_t* cur_op;

		if((DWORD)offset_in >= size_in
			/*|| (entry_new && state.entry == entry_count - 1)*/
			// (sounds like a good idea, but breaks th09 Marisa)
		) {
			break;
		}
		if(state.cmd_in->time == 0 && state.cmd_in->type == 0) {
			// Last command of the entry
			// Next command will start a new one
			entry_new = 1;

			// Close any open text boxes.
			// Very important - if the last box of an entry should receive a new line,
			// it would otherwise be created in the context of the next entry,
			// messing up the entry offsets in the process.
			// Fixes bug #3 (https://bitbucket.org/nmlgc/thpatch-bugs/issue/3)
			box_end(&state);
		}

		if(entry_new && state.cmd_in->type != 0) {
			// First command of a new entry.
			// Get entry object from the diff file,
			// and assign the correct offset to the output buffer.
			size_t i;
			for(i = 0; i < entry_count; ++i) {
				if(offset_in == entry_offsets_in[i * entry_offset_mul]) {
					state.entry = i;
					state.diff_entry = json_object_numkey_get(patch, state.entry);
					entry_offsets_out[i * entry_offset_mul] = offset_out;
					break;
				}
			}
			entry_new = 0;
			state.time = -1;
		}

		if(state.cmd_in->time != state.time) {
			// New timecode. Reset index
			state.time = state.cmd_in->time;
			state.ind = -1;
		}

		// Firstly, copy this command to the output buffer
		// If there's anything to change, we do so later
		memcpy(state.cmd_out, state.cmd_in, sizeof(th06_msg_t) + state.cmd_in->length);

		// Look up what to do with the opcode...
		cur_op = get_op_info(&state, state.cmd_in->type);

		if(cur_op && json_is_object(state.diff_entry)) {
			advance_out = process_op(cur_op, &state);
		}

		state.cmd_in = (th06_msg_t*)((BYTE*)state.cmd_in + th06_msg_full_len(state.cmd_in));
		if(advance_out) {
			state.cmd_out = (th06_msg_t*)((BYTE*)state.cmd_out + th06_msg_full_len(state.cmd_out));
		}
	}

#ifdef _DEBUG
	{
		FILE* out = fopen("out.msg", "wb");
		if(out) {
			fwrite(msg_out, (BYTE*)state.cmd_out - (BYTE*)msg_out, 1, out);
			fclose(out);
		}
	}
#endif
	HeapFree(GetProcessHeap(), 0, msg_in);
	patch_msg_state_clear(&state);
	return 0;
}

int patch_msg_dlg(BYTE *file_inout, size_t size_out, size_t size_in, json_t *patch, json_t *run_cfg)
{
	return patch_msg(file_inout, size_out, size_in, patch, runconfig_format_get("msg"));
}

int patch_msg_end(BYTE *file_inout, size_t size_out, size_t size_in, json_t *patch, json_t *run_cfg)
{
	return patch_msg(file_inout, size_out, size_in, patch, runconfig_format_get("end"));
}
