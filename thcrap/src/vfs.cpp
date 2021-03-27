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
	size_t i = handler.out_pattern.find('\\');
	while (i != std::string::npos) {
		handler.out_pattern[i] = '/';
		i = out_pattern.find('\\');
	}
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
	str_slash_normalize(str);
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

	VLA(char, fn_normalized, fn.length() + 1);
	strcpy(fn_normalized, fn.c_str());
	str_slash_normalize(fn_normalized);
	for (auto& handler : vfs_handlers) {
		if (PathMatchSpec(fn_normalized, handler.out_pattern.c_str())) {
			std::unordered_map<std::string, json_t *> in_data;
			for (auto& s : handler.in_fns) {
				in_data[s] = jsondata_get(s.c_str());
			}

			size_t cur_size = 0;
			json_t *new_obj = handler.gen(in_data, fn_normalized, &cur_size);
			obj = json_object_merge(obj, new_obj);
			if (size) {
				*size += cur_size;
			}
		}
	}

	VLA_FREE(fn_normalized);
	return obj;
}



static json_t *json_map_resolve(json_t *obj, const char *path)
{
	if (!path || !path[0]) {
		return NULL;
	}

	char *dup = strdup(path);
	char *cur = dup;
	char *next;
	while ((next = strchr(dup, '.'))) {
		*next = '\0';
		next++;
		obj = json_object_get(obj, cur);
		if (!obj) {
			free(dup);
			return NULL;
		}
		cur = next;
	}
	obj = json_object_get(obj, cur);
	free(dup);
	return obj;
}

static json_t *json_map_patch(json_t *map, json_t *in)
{
	json_t *ret;
	json_t *it;
	if (json_is_object(map)) {
		ret = json_object();
		const char *key;
		json_object_foreach(map, key, it) {
			if (json_is_object(it) || json_is_array(it)) {
				json_object_set_new(ret, key, json_map_patch(it, in));
			}
			else if (json_is_string(it)) {
				json_t *rep = json_map_resolve(in, json_string_value(it));
				if (rep) {
					json_object_set(ret, key, rep);
				}
			}
		}
	}
	else if (json_is_array(map)) {
		ret = json_array();
		size_t i;
		json_array_foreach(map, i, it) {
			if (json_is_object(it) || json_is_array(it)) {
				json_array_append_new(ret, json_map_patch(it, in));
			}
			else if (json_is_string(it)) {
				json_t *rep = json_map_resolve(in, json_string_value(it));
				if (rep) {
					json_array_append(ret, rep);
				}
			}
		}
	}
	return ret;
}

static json_t *map_generator(std::unordered_map<std::string, json_t*> in_data, const std::string out_fn, size_t* out_size)
{
	if (out_size) {
		*out_size = 0;
	}

	if (in_data.size() == 0) {
		return NULL;
	}
	json_t *in = in_data.begin()->second;

	const std::string map_fn = out_fn.substr(0, out_fn.size() - 6) + ".map";
	json_t *map = stack_json_resolve(map_fn.c_str(), out_size);
	if (map == NULL) {
		return NULL;
	}

	return json_map_patch(map, in);
}

void jsonvfs_add_map(const std::string out_pattern, std::string in_fn)
{
	jsonvfs_add(out_pattern, { in_fn }, map_generator);
}

void jsonvfs_game_add_map(const std::string out_pattern, std::string in_fn)
{
	jsonvfs_game_add(out_pattern, { in_fn }, map_generator);
}
