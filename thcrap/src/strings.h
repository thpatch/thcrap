/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Persistent string storage, and translation of hardcoded strings.
  */

#pragma once

/// Translation
/// -----------
// Returns the ID for the given [str], or NULL if it couldn't be determined.
// Currently only looking up the pointer value in the stringlocs map.
THCRAP_API const char* strings_id(const char* str);

// Returns [id] from the string definition table,
// or NULL if no string for [id] is available.
THCRAP_API const json_t* strings_get(const char *id);

#ifdef __cplusplus
extern "C++" {

// Like strings_get(), but with a fallback onto [def] if no string for [id]
// is available.
struct string_named_t {
	const char *id;
	const stringref_t fallback;
};

THCRAP_API stringref_t strings_get_fallback(const string_named_t& sn);

}
#endif

// Returns the translated string for [in] from the string definition table,
// or [in] itself if no translation is available.
// Optionally returns the length of the returned string in [out_len], without
// the terminating \0, if [out_len] is not a nullptr.
THCRAP_API const char* TH_CDECL strings_lookup(const char *in, size_t *out_len);

// String lookup for variable argument lists. Parses [format] and calls
// [strings_lookup] for every string parameter in [va].
THCRAP_API void strings_va_lookup(va_list va, const char *format);
/// -----------

/// Persistent storage
/// ------------------
// Returns a pointer to the string in the persistent storage slot [slot],
// or NULL on allocation failure. The string's buffer is automatically
// resized to contain at least [min_len] bytes and a null terminator.
THCRAP_API char* strings_storage_get(const size_t slot, size_t min_len);

// Safe and persistent sprintf handler.
// This function should be inserted via binary hacks everywhere the patched
// program calls sprintf, as it guarantees a sufficiently large buffer for the
// result. [format] and [va] are the respective parameters of vsprintf. The
// storage [slot] can be any value, but calling this function again with the
// same [slot] deletes the result from the previous call.
// Returns a pointer to the resulting string.
THCRAP_API const char* strings_vsprintf(const size_t slot, const char *format, va_list va);
THCRAP_API const char* strings_sprintf(const size_t slot, const char *format, ...);

THCRAP_API const char* strings_vsprintf_msvcrt14(const char *format, const size_t slot, va_list va);

// Clears the string in [slot].
THCRAP_API const char* strings_strclr(const size_t slot);

// Concatenates [src] to the string in [slot] or initializes [slot] if it
// is currently empty.
THCRAP_API const char* strings_strcat(const size_t slot, const char *src);

// Replaces all occurences of [src] in [slot] with [dst]. If [dst] is NULL,
// the function defaults it to an empty string, thus removing all occurences
// of [src]. Currently, this function doesn't do translation lookup since we
// don't yet have any specific use cases.
THCRAP_API const char* strings_replace(const size_t slot, const char *src, const char *dst);
/// ------------------

#if __cplusplus

extern "C++" {

template<typename T = void>
struct fixed_string_list {

	size_t count;
	char** strs;

	static inline constexpr size_t NOT_FOUND = size_t_neg_one;

	fixed_string_list() = default;

	void update_max_count(size_t count) {}

	void initialize(const std::vector<std::string_view>& strs_in) {
		size_t count = this->count = strs_in.size();
		if constexpr (!std::is_same_v<T, void>) {
			((T*)this)->update_max_count(count);
		}
		size_t buffer = 0;
		for (const auto& str : strs_in) {
			buffer += str.length() + 1;
		}
		char** strs = this->strs = (char**)malloc(count * sizeof(char*) + buffer);
		char* str_raw = (char*)&strs[count];
		for (const auto& str : strs_in) {
			*strs++ = str_raw;
			size_t length = str.length() + 1;
			memcpy(str_raw, str.data(), length);
			str_raw += length;
		}
	}

	void initialize(char** strs_in, size_t count) {
		this->count = count;
		if constexpr (!std::is_same_v<T, void>) {
			((T*)this)->update_max_count(count);
		}
		size_t buffer = 0;
		for (size_t i = 0; i < count; ++i) {
			buffer += strlen(strs_in[i]) + 1;
		}
		char** strs = this->strs = (char**)malloc(count * sizeof(char*) + buffer);
		char* str_raw = (char*)&strs[count];
		for (size_t i = 0; i < count; ++i) {
			strs[i] = str_raw;
			size_t length = strlen(strs_in[i]) + 1;
			memcpy(str_raw, strs_in[i], length);
			str_raw += length;
		}
	}

	fixed_string_list(const std::vector<std::string_view>& strs) {
		this->initialize(strs);
	}

	fixed_string_list(char** strs, size_t count) {
		this->initialize(strs, count);
	}

	size_t find(std::string_view str) const {
		size_t count = this->count;
		char** strs = this->strs;
		for (size_t i = 0; i < count; ++i) {
			if (str == strs[i]) {
				return i;
			}
		}
		return NOT_FOUND;
	}
};

}

#endif
