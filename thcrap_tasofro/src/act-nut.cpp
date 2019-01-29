/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Patching of ACT and NUT files.
  */

#include <thcrap.h>
#include <libs\135tk\Act-Nut-lib\exports.hpp>
#include "thcrap_tasofro.h"
#include "act-nut.h"

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
	json_t *flexarray;
	json_object_foreach(patch, key, flexarray) {
		std::string text;
		size_t ind;
		json_t *line;
		json_flex_array_foreach(flexarray, ind, line) {
			if (text.length() > 0) {
				text += "\n";
			}
			text += json_string_value(line);
		}

		ActNut::Object *child = actnutobj->getChild(key);
		if (child) {
			*child = text;
		}
		else {
			log_printf("Act/Nut: key %s not found\n", key);
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
