/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Patching of ACT and NUT files.
  */

#include <thcrap.h>
#include <libs/135tk/Act-Nut-lib/exports.hpp>
#include "thcrap_tasofro.h"
#include "act-nut.h"

static void patch_actnut_as_object(ActNut::Object* elem, json_t *json)
{
	size_t offset_for_instructions = 0;

	json_t *insert_instructions = json_object_get(json, "insert_instructions");
	if (insert_instructions) {
		Nut::SQFunctionProto *func = dynamic_cast<Nut::SQFunctionProto*>(elem);
		if (func) {
			const char *instruction_number_s;
			json_t *instruction_flexarray;
			json_object_foreach(insert_instructions, instruction_number_s, instruction_flexarray) {
				size_t instruction_number = atoi(instruction_number_s);
				size_t i;
				json_t *instruction;
				json_flex_array_foreach(instruction_flexarray, i, instruction) {
					func->insertInstruction(instruction_number + offset_for_instructions, json_string_value(instruction));
					offset_for_instructions++;
				}
			}
		}
		else {
			log_print("Act/Nut: insert_instructions found for a non-function element\n");
		}
	}

	json_t *remove_instructions = json_object_get(json, "remove_instructions");
	if (remove_instructions) {
		Nut::SQFunctionProto *func = dynamic_cast<Nut::SQFunctionProto*>(elem);
		if (func) {
			Nut::vector *instructions = dynamic_cast<Nut::vector*>((*func)["instructions"]);

			size_t i;
			json_t *value;
			json_flex_array_foreach(remove_instructions, i, value) {
				size_t line = (size_t)json_integer_value(value);
				if (line + offset_for_instructions >= instructions->size()) {
					log_printf("Act/Nut: insert_instructions: instruction number too big (%u)\n", line);
					continue;
				}

				Nut::SQInstruction *instruction = dynamic_cast<Nut::SQInstruction*>((*instructions)[line + offset_for_instructions]);
				// line instructions are used only when a Squirrel debugger is present. We use them as nop.
				*(*instruction)[0] = "line";
			}
		}
	}
}

static void patch_actnut_as_string(ActNut::Object *elem, json_t *json)
{
	std::string text;
	size_t i;
	json_t *line;

	json_flex_array_foreach(json, i, line) {
		if (text.length() > 0) {
			text += "\n";
		}
		text += json_string_value(line);
	}
	*elem = text;
}

int patch_act_nut(ActNut::Object *actnutobj, void *file_out, size_t size_out, json_t *patch)
{
	if (patch == nullptr) {
		// Nothing to do
		return 0;
	}

	if (actnutobj == nullptr) {
		log_print("Act/Nut: parsing failed\n");
		return -1;
	}

	const char *key;
	json_t *value;
	json_object_foreach(patch, key, value) {
		ActNut::Object *child = actnutobj->getChild(key);
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
	Act::File *file = Act::read_act_from_bytes((uint8_t *)file_inout, size_in);
	return patch_act_nut(file, file_inout, size_out, patch);
}

int patch_nut(void *file_inout, size_t size_out, size_t size_in, const char*, json_t *patch)
{
	Nut::Stream *stream = Nut::read_nut_from_bytes((uint8_t *)file_inout, size_in);
	return patch_act_nut(stream, file_inout, size_out, patch);
}
