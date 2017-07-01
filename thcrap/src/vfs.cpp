/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Virtual file system.
  */

#include "thcrap.h"
#include "vfs.h"
#include <vector>

typedef struct
{
	std::string out_pattern;
	std::unordered_set<std::string> in_fns;
	jsonvfs_generator_t *gen;
} jsonvfs_handler_t;

static std::vector<jsonvfs_handler_t> vfs_handlers;

void jsonvfs_add(const std::string out_pattern, std::unordered_set<std::string> in_fns, jsonvfs_generator_t *gen)
{
	jsonvfs_handler_t handler {
		out_pattern,
		in_fns,
		gen
	};
	for (auto& s : in_fns) {
		jsondata_add(s.c_str());
	}
	vfs_handlers.push_back(handler);
}

void jsonvfs_game_add(const std::string out_pattern, std::unordered_set<std::string> in_fns, jsonvfs_generator_t *gen)
{
	jsonvfs_handler_t handler;
	char *str;

	str = fn_for_game(out_pattern.c_str());
	handler.out_pattern = str;
	SAFE_FREE(str);

	for (auto& s : in_fns) {
		str = fn_for_game(s.c_str());
		handler.in_fns.insert(str);
		jsondata_add(str);
		SAFE_FREE(str);
	}
	handler.gen = gen;

	vfs_handlers.push_back(handler);
}

json_t *jsonvfs_get(const std::string fn, size_t* size)
{
	json_t *obj = NULL;
	if (size) {
		*size = 0;
	}

	for (auto& handler : vfs_handlers) {
		if (PathMatchSpec(fn.c_str(), handler.out_pattern.c_str())) {
			std::unordered_map<std::string, json_t *> in_data;
			for (auto& s : handler.in_fns) {
				in_data[s] = jsondata_get(s.c_str());
			}

			size_t cur_size = 0;
			json_t *new_obj = handler.gen(in_data, fn, &cur_size);
			if (!obj) {
				obj = new_obj;
			}
			else {
				json_object_merge(obj, new_obj);
				json_decref(new_obj);
			}
			if (size) {
				*size += cur_size;
			}
		}
	}

	return obj;
}