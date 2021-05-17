/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * JSON data storage for modules.
  */

#include "thcrap.h"
#include <forward_list>

/**
  * This module provides a simple container for other modules to store their
  * custom JSON data. Not only does this remove the need for modules to
  * resolve, keep, and free their data in most cases, it also provides
  * automatic, transparent and thread-safe repatching.
  *
  * Instead of simply storing the contents of every file directly as a value
  * to its file name key, it is wrapped into a forward list. To repatch a file,
  * jsondata_mod_repatch() then simply inserts the new version at the front
  * of the list. Because forwards lists never invalidate references when adding
  * new elements, jsondata_get() is able to return a constant reference to the
  * most current version of a file merely by referencing the first element.
  *
  * This is very important, given that passing constant memory addresses to
  * strings back to the game is one of the main uses of custom JSON data in
  * the first place. Requiring every caller to do reference counting would
  * render this impossible, aside from also complicating the usage of this
  * module.
  *
  * However, this also means that every old version of a file will be kept in
  * memory until the destructor of jsondata is called. I see no straightforward
  * way to safely clean up unused references, short of the heap inspection methods
  * used by garbage collectors.
  */

std::unordered_map<std::string_view, std::forward_list<UniqueJsonPtr>> jsondata;

void jsondata_add(const char* fn) {
	auto existing = jsondata.find(fn);
	auto& list = (existing == jsondata.end()) ? jsondata[strdup(fn)] : existing->second;
	list.emplace_front(stack_json_resolve(fn, NULL));
}

void jsondata_game_add(const char* fn) {
	if (char* game_fn = fn_for_game(fn)) {
		jsondata_add(game_fn);
		free(game_fn);
	}
}

json_t* jsondata_get(const char* fn) {
	auto existing = jsondata.find(fn);
	if (existing != jsondata.end()) {
		return existing->second.front().get();
	}
	return NULL;
}

json_t* jsondata_game_get(const char* fn) {
	if (char* game_fn = fn_for_game(fn)) {
		json_t* ret = jsondata_get(game_fn);
		free(game_fn);
		return ret;
	}
	return NULL;
}

//void jsondata_mod_repatch(const char* files_changed[]) {
//	while (const char* fn = *files_changed++) {
//		auto existing = jsondata.find(fn);
//		if (existing != jsondata.end()) {
//			existing->second.emplace_front(stack_json_resolve(fn, NULL));
//		}
//	}
//}

void jsondata_mod_repatch(json_t* files_changed) {
	const char* fn;
	json_t* json;
	json_object_foreach(files_changed, fn, json) {
		auto existing = jsondata.find(fn);
		if (existing != jsondata.end()) {
			existing->second.emplace_front(stack_json_resolve(fn, NULL));
		}
	}
}
