/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Patching of ACT and NUT files.
  */

#include <thcrap.h>
#include <algorithm>
#include <libs/135tk/Act-Nut-lib/exports.hpp>
#include "thcrap_tasofro.h"
#include "act-nut.h"

void json_object_numkeys_foreach(json_t *obj, std::function<void (int key, json_t *value)> function)
{
	std::vector<int> keys;

	for (void *iter = json_object_iter(obj); iter != nullptr; iter = json_object_iter_next(obj, iter)) {
		const char *key = json_object_iter_key(iter);
		keys.push_back(atoi(key));
	}

	std::sort(keys.begin(), keys.end());
	for (int key : keys) {
		std::string key_s = std::to_string(key);
		function(key, json_object_get(obj, key_s.c_str()));
	}
}

static void patch_SQFunctionProto(Nut::SQFunctionProto *func, json_t *add_literals, json_t *replace_instructions, json_t *insert_instructions, json_t *remove_instructions)
{
	// Add literals to the function. They can be used by instructions in insert_instructions and set_instructions.
	if (add_literals) {
		json_t *value;
		json_flex_array_foreach_scoped(size_t, i, add_literals, value) {
			func->addLiteral(json_string_value(value));
		}
	}

	// Remove instructions. They aren't removed (only replaced by nop instructions), so you don't need to change any offset.
	if (remove_instructions) {
		Nut::vector *instructions = dynamic_cast<Nut::vector*>((*func)["Instructions"]);

		json_t *value;
		json_flex_array_foreach_scoped(size_t, i, remove_instructions, value) {
			size_t line = (size_t)json_integer_value(value);
			if (line >= instructions->size()) {
				log_printf("Act/Nut: insert_instructions: instruction number too big (%u)\n", line);
				continue;
			}

			Nut::SQInstruction *instruction = dynamic_cast<Nut::SQInstruction*>((*instructions)[line]);
			// line instructions are used only when a Squirrel debugger is present. We use them as nop.
			*(*instruction)["0"] = "line";
			*(*instruction)["2"] = "0";
		}
	}

	// Replace instructions. Basically the same as setting "/Instructions/X" from the root object,
	// but this one is applied when it makes the most sense: after literals insertion (so they are
	// available), but before instructions insertion (so the instruction offsets don't change).
	if (replace_instructions) {
		Nut::vector *instructions = dynamic_cast<Nut::vector*>((*func)["Instructions"]);

		const char *key;
		json_t *value;
		json_object_foreach_fast(replace_instructions, key, value) {
			ActNut::Object *instruction = (*instructions)[key];
			if (!instruction) {
				log_printf("Act/Nut: set_instructions: instruction not found (%s)\n", key);
				continue;
			}

			*instruction = json_string_value(value);
		}
	}

	// Insert new instructions.
	// The offsets are always relative to the original file.
	// Note that you will need to fix jump targets if you insert instructions between a jump and its target.
	if (insert_instructions) {
		size_t offset_for_instructions = 0;

		json_object_numkeys_foreach(insert_instructions, [func, &offset_for_instructions](int instruction_number, json_t *instruction_flexarray) {
			json_t *instruction;
			json_flex_array_foreach_scoped(size_t, i, instruction_flexarray, instruction) {
				func->insertInstruction(instruction_number + offset_for_instructions, json_string_value(instruction));
				offset_for_instructions++;
			}
		});
	}
}

static void patch_actnut_as_object(ActNut::Object* elem, json_t *json)
{
	json_t *add_literals = json_object_get(json, "add_literals");
	json_t *replace_instructions = json_object_get(json, "replace_instructions");
	json_t *insert_instructions = json_object_get(json, "insert_instructions");
	json_t *remove_instructions = json_object_get(json, "remove_instructions");

	if (add_literals || replace_instructions || insert_instructions || remove_instructions) {
		Nut::SQFunctionProto *func = dynamic_cast<Nut::SQFunctionProto*>(elem);
		if (func) {
			patch_SQFunctionProto(func, add_literals, replace_instructions, insert_instructions, remove_instructions);
		}
		else {
			log_print("Act/Nut: function modificator found for a non-function element\n");
		}
	}
}

static void patch_actnut_as_string(ActNut::Object *elem, json_t *json)
{
	std::string text;
	json_t *line;

	json_flex_array_foreach_scoped(size_t, i, json, line) {
		if (text.length() > 0) {
			text += "\n";
		}
		text += json_string_value(line);
	}
	*elem = parse_ruby(text);
}

int patch_act_nut(ActNut::Object *actnutobj, void *file_out, size_t size_out, json_t *patch)
{
	if (actnutobj == nullptr) {
		log_print("Act/Nut: parsing failed\n");
		return -1;
	}

	const char *key;
	json_t *value;
	json_object_foreach_fast(patch, key, value) {
		ActNut::Object *child;
		if (strcmp(key, "/") == 0) {
			child = actnutobj;
		}
		else {
			child = actnutobj->getChild(key);
		}
		if (!child) {
			log_printf("Act/Nut: key %s not found\n", key);
			continue;
		}

		if (json_is_object(value)) {
			patch_actnut_as_object(child, value);
		}
		else if (json_is_string(value) || json_is_array(value)) {
			patch_actnut_as_string(child, value);
		}
		else {
			log_printf("Act/Nut: %s: unsupported json type\n", key);
		}
	}

	ActNut::MemoryBuffer *buf = ActNut::new_MemoryBuffer(ActNut::MemoryBuffer::SHARE, (uint8_t *)file_out, size_out, false);
	if (!actnutobj->writeValue(*buf)) {
		log_print("Act/Nut: writing failed\n");
		ActNut::delete_buffer(buf);
		ActNut::delete_object(actnutobj);
		return -1;
	}
	ActNut::delete_buffer(buf);
	ActNut::delete_object(actnutobj);
	return 1;
}

int patch_act(void *file_inout, size_t size_out, size_t size_in, const char*, json_t *patch)
{
	if (patch == nullptr) {
		// Nothing to do
		return 0;
	}

	Act::File *file = Act::read_act_from_bytes((uint8_t *)file_inout, size_in);
	return patch_act_nut(file, file_inout, size_out, patch);
}

int patch_nut(void *file_inout, size_t size_out, size_t size_in, const char*, json_t *patch)
{
	if (patch == nullptr) {
		// Nothing to do
		return 0;
	}

	Nut::Stream *stream = Nut::read_nut_from_bytes((uint8_t *)file_inout, size_in);
	return patch_act_nut(stream, file_inout, size_out, patch);
}
