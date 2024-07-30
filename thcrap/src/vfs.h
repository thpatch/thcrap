/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Virtual file system.
  */

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

struct jsonvfs_files : public fixed_string_list<jsonvfs_files> {

	using fixed_string_list<jsonvfs_files>::fixed_string_list;

	static inline size_t max_count = 0;

	inline void update_max_count(size_t count) {
		if (count > max_count) {
			max_count = count;
		}
	}
};

struct jsonvfs_map_iterator {
	size_t index;

	jsonvfs_map_iterator(size_t index) : index(index) {}

	bool operator!=(jsonvfs_map_iterator rhs) const {
		return this->index != rhs.index;
	}

	jsonvfs_map_iterator& operator++() {
		++this->index;
		return *this;
	}

	size_t operator*() const {
		return this->index;
	}
};

struct jsonvfs_map {
	jsonvfs_files files;
	json_t* json_data[];

	TH_FORCEINLINE jsonvfs_map(const jsonvfs_files& files) {
		size_t count = this->files.count = files.count;
		char** strs = this->files.strs = files.strs;
		for (size_t i = 0; i < count; ++i) {
			this->json_data[i] = jsondata_get(strs[i]);
		}
	}

	json_t* find(std::string_view str) const {
		size_t index = this->files.find(str);
		if (index != fixed_string_list<>::NOT_FOUND) {
			return this->json_data[index];
		}
		return NULL;
	}

	json_t* operator[](std::string_view str) const {
		return this->find(str);
	}

	json_t* operator[](size_t index) const {
		return this->json_data[index];
	}

	bool empty() const {
		return !this->files.count;
	}

	jsonvfs_map_iterator begin() const {
		return 0;
	}

	jsonvfs_map_iterator end() const {
		return this->files.count;
	}
};

typedef json_t* jsonvfs_generator_t(const jsonvfs_map& in_data, std::string_view out_fn, size_t& out_size);

extern "C" {
	/**
	  * Add a handler to create files in the virtual file system.
	  *
	  * in_fns contains a list of json files needed to generate the virtual files.
	  *
	  * Each time a jdiff file matching out_pattern is resolved, gen will be called.
	  * If it returns a non-NULL value, the return value will be used as if a file
	  * with this content exists at the top of the patch stack.
	  */
	THCRAP_API void jsonvfs_add(const char* out_pattern, const std::vector<std::string_view>& in_fns, jsonvfs_generator_t *gen);

	// Same as jsonvfs_add, but all file names are game-relative.
	THCRAP_API void jsonvfs_game_add(const char* out_pattern, const std::vector<std::string_view>& in_fns, jsonvfs_generator_t *gen);

	/**
	  * Generate a VFS file from a JSON map file.
	  * The map file is called <filename>.map and will generate a file called <filename>.jdiff.
	  * The map file is a json file, with strings being references to JSON values in in_fn.
	  * For example, if you have this in the map file:
	  * { "key": "object.example" }
	  * and this in in_fn:
	  * { "object": { "example": 5 } }
	  * the generated jdiff file will be like this:
	  * { "key": 5 }
	  */
	THCRAP_API void jsonvfs_add_map(const char* out_pattern, const std::vector<std::string_view>& in_fns);

	// Same as jsonvfs_add_map, but all file names are game-relative.
	THCRAP_API void jsonvfs_game_add_map(const char* out_pattern, const std::vector<std::string_view>& in_fns);


	// Return a file from the vfs if it exists.
	THCRAP_API json_t *jsonvfs_get(const char* fn, size_t* size);
}
