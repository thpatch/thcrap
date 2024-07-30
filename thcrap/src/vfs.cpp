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

struct jsonvfs_handler_t {
	const wchar_t* out_pattern;
	jsonvfs_files in_fns;
	jsonvfs_generator_t *gen;

	jsonvfs_handler_t(const wchar_t* out_pattern, const std::vector<std::string_view>& in_fns, jsonvfs_generator_t* gen)
		: out_pattern(out_pattern), in_fns(in_fns), gen(gen)
	{}

	jsonvfs_handler_t(const wchar_t* out_pattern, char** strs, size_t count, jsonvfs_generator_t* gen)
		: out_pattern(out_pattern), in_fns(strs, count), gen(gen)
	{}
};

static std::vector<jsonvfs_handler_t> vfs_handlers;

void jsonvfs_add(const char* out_pattern, const std::vector<std::string_view>& in_fns, jsonvfs_generator_t *gen)
{
	for (auto& s : in_fns) {
		jsondata_add(s.data());
	}
	wchar_t* wstr = (wchar_t*)utf8_to_utf16(out_pattern);
	wstr_slash_normalize(wstr);
	vfs_handlers.emplace_back( wstr, in_fns, gen );
}

void jsonvfs_game_add(const char* out_pattern, const std::vector<std::string_view>& in_fns, jsonvfs_generator_t *gen)
{
	char* str = fn_for_game(out_pattern);
	str_slash_normalize(str);
	wchar_t* wstr = (wchar_t*)utf8_to_utf16(str);
	free(str);

	size_t count = in_fns.size();
	VLA(char*, fns_for_game, count);

	for (size_t i = 0; i < count; ++i) {
		char* fn = fn_for_game(in_fns[i].data());
		fns_for_game[i] = fn;
		jsondata_add(fn);
	}

	vfs_handlers.emplace_back( wstr, fns_for_game, count, gen );

	for (size_t i = 0; i < count; ++i) {
		free(fns_for_game[i]);
	}

	VLA_FREE(fns_for_game);
}

json_t *jsonvfs_get(const char* fn, size_t* size)
{
	json_t *obj = NULL;
	size_t total_size = 0;

	if (!vfs_handlers.empty()) {

		size_t fn_len = strlen(fn);
		VLA(char, fn_normalized, fn_len + 1);
		for (size_t i = 0; i < fn_len + 1; ++i) {
			char c = fn[i];
			fn_normalized[i] = c != '\\' ? c : '/';
		}
		std::string_view fn_view{ fn_normalized, fn_len };

		VLA(wchar_t, fn_normalized_w, fn_len + 1);
		StringToUTF16(fn_normalized_w, fn_normalized, fn_len + 1);

		jsonvfs_map* vfs_map = (jsonvfs_map*)_alloca(sizeof(jsonvfs_map) + sizeof(json_t*) * jsonvfs_files::max_count);

		for (auto& handler : vfs_handlers) {
			if (PathMatchSpecExW(fn_normalized_w, handler.out_pattern, PMSF_NORMAL) == S_OK) {

				new (vfs_map) jsonvfs_map(handler.in_fns);

				size_t cur_size = 0;
				json_t *new_obj = handler.gen(*vfs_map, fn_view, cur_size);
				total_size += cur_size;
				obj = json_object_merge(obj, new_obj);
			}
		}
		VLA_FREE(fn_normalized);
		VLA_FREE(fn_normalized_w);
	}

	if (size) {
		*size = total_size;
	}

	return obj;
}



static json_t *json_map_resolve(json_t *obj, const char *path)
{
	if unexpected(!path) {
		return NULL;
	}
	for (const char* cur_start = path;; ++path) {
		switch (char c = *path) {
			case '.': case '\0':
				obj = json_object_getn(obj, cur_start, PtrDiffStrlen(path, cur_start));
				if (!obj || !c) {
					return obj;
				}
				cur_start = path + 1;
		}
	}
}

static json_t *json_map_patch(json_t *map, json_t *in)
{
	json_t *ret;
	json_t *it;
	if (json_is_object(map)) {
		ret = json_object();
		const char *key;
		json_object_foreach_fast(map, key, it) {
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
		json_array_foreach_scoped(size_t, i, map, it) {
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

static json_t *map_generator(const jsonvfs_map& in_data, std::string_view out_fn, size_t& out_size)
{
	const std::string map_fn = std::string(out_fn.substr(0, out_fn.size() - 5)) + "map";
	json_t *map = stack_json_resolve_vfs(map_fn.c_str(), &out_size);
	if (map) {
		json_t *patch_full = nullptr;
		for (auto i : in_data) {
			json_t *patch = json_map_patch(map, in_data[i]);
			patch_full = json_object_merge(patch_full, patch);
		}
		return patch_full;
	}
	return NULL;
	
}

void jsonvfs_add_map(const char* out_pattern, const std::vector<std::string_view>& in_fns)
{
	jsonvfs_add(out_pattern, in_fns, map_generator);
}

void jsonvfs_game_add_map(const char* out_pattern, const std::vector<std::string_view>& in_fns)
{
	jsonvfs_game_add(out_pattern, in_fns, map_generator);
}
