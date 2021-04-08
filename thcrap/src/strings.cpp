/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Persistent string storage, and translation of hardcoded strings.
  */

#include "thcrap.h"

/// Detour chains
/// -------------
W32U8_DETOUR_CHAIN_DEF(MessageBox);
W32U8_DETOUR_CHAIN_DEF(FindFirstFile);
W32U8_DETOUR_CHAIN_DEF(CreateFile);

/// -------------

// Since we can't use the jsondata module to make this repatchable,
// we only need to keep the last parsed JSON object around to
// provide the "backing memory" for the ID strings.
json_t *backing_obj = nullptr;

// Length-prefixed string object used for persistent storage
typedef struct {
	size_t len;
	char str;
} storage_string_t;

std::unordered_map<size_t, storage_string_t *> strings_storage;

SRWLOCK stringlocs_srwlock = {SRWLOCK_INIT};
std::unordered_map<const char *, const char *> stringlocs;

#define addr_key_len 2 + (sizeof(void*) * 2) + 1

void stringlocs_reparse(void)
{
	json_t* new_obj = stack_game_json_resolve("stringlocs.js", NULL);
	const char *key;
	const json_t *val;

	AcquireSRWLockExclusive(&stringlocs_srwlock);
	stringlocs.clear();

	json_object_foreach(new_obj, key, val) {
		// TODO: For now, we're nagging developers with one message box for
		// every single parse error. It'd certainly be better if we gathered
		// all errors into a single message box to be printed at the end of
		// the parsing, and even had a somewhat generic solution if we do
		// more of these conversions.
#define stringlocs_log_error(msg) \
	log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION, \
		"Error parsing stringlocs.js: \"%s\" " msg".", key, sizeof(size_t) * 8 \
	)

		if(!json_is_string(val)) {
			stringlocs_log_error("must be a JSON string");
			continue;
		}
		str_address_ret_t addr_ret;
		auto *addr = (const char *)str_address_value(key, NULL, &addr_ret);
		if(addr_ret.error == STR_ADDRESS_ERROR_NONE) {
			stringlocs[addr] = json_string_value(val);
		}
		if(addr_ret.error & STR_ADDRESS_ERROR_OVERFLOW) {
			stringlocs_log_error("exceeds %d bits");
		}
		if(addr_ret.error & STR_ADDRESS_ERROR_GARBAGE) {
			stringlocs_log_error("has garbage at the end");
		}
#undef stringlocs_log_error
	}

	json_decref(backing_obj);
	backing_obj = new_obj;

	ReleaseSRWLockExclusive(&stringlocs_srwlock);
}

const char* strings_id(const char *str)
{
	const char *ret = nullptr;
	AcquireSRWLockShared(&stringlocs_srwlock);
	auto id_key = stringlocs.find(str);
	if(id_key != stringlocs.end()) {
		ret = id_key->second;
	}
	ReleaseSRWLockShared(&stringlocs_srwlock);
	return ret;
}

const json_t* strings_get(const char *id)
{
	return json_object_get(jsondata_get("stringdefs.js"), id);
}

stringref_t strings_get_fallback(const string_named_t& sn)
{
	auto ret = strings_get(sn.id);
	if(!json_is_string(ret)) {
		return sn.fallback;
	}
	return ret;
}

const char* __cdecl strings_lookup(const char *in, size_t *out_len)
{
	const char *ret = in;

	if(!in) {
		return in;
	}

	AcquireSRWLockShared(&stringlocs_srwlock);
	auto id = strings_id(in);
	if(id) {
		auto *new_str = json_string_value(strings_get(id));
		if(new_str && new_str[0]) {
			ret = new_str;
		}
	}
	ReleaseSRWLockShared(&stringlocs_srwlock);

	if(out_len) {
		*out_len = strlen(ret);
	}
	return ret;
}

void strings_va_lookup(va_list va, const char *format)
{
	const char *p = format;
	while(*p) {
		printf_format_t fmt;
		int i;

		// Skip characters before '%'
		for(; *p && *p != '%'; p++);
		if(!*p) {
			break;
		}
		// *p == '%' here
		p++;

		// output a single '%' character
		if(*p == '%') {
			p++;
			continue;
		}
		p = printf_format_parse(&fmt, p);
		for(i = 0; i < fmt.argc_before_type; i++) {
			va_arg(va, int);
		}
		if(fmt.type == 's' || fmt.type == 'S') {
			*(const char**)va = strings_lookup(*(const char**)va, NULL);
		}
		for(i = 0; i < fmt.type_size_in_ints; i++) {
			va_arg(va, int);
		}
	}
}

char* strings_storage_get(const size_t slot, size_t min_len)
{
	auto stored = strings_storage.find(slot);
	auto *ret = stored != strings_storage.end() ? stored->second : nullptr;

	// MSVCRT's realloc implementation moves the buffer every time, even if the
	// new length is shorter...
	if(ret == nullptr || (min_len && ret->len < min_len)) {
		auto *ret_new = (storage_string_t*)realloc(ret, min_len + sizeof(storage_string_t));
		// Yes, this correctly handles a realloc failure.
		if(ret_new) {
			ret_new->len = min_len;
			if(!ret) {
				ret_new->str = 0;
			}
			strings_storage[slot] = ret_new;
			ret = ret_new;
		}
	}
	if(ret) {
		return &ret->str;
	}
	return nullptr;
}

const char* strings_vsprintf(const size_t slot, const char *format, va_list va)
{
	char *ret = NULL;
	size_t str_len;

	format = strings_lookup(format, NULL);
	strings_va_lookup(va, format);

	if(!format) {
		return NULL;
	}
	str_len = _vscprintf(format, va) + 1;

	ret = strings_storage_get(slot, str_len);
	if(ret) {
		vsprintf(ret, format, va);
		return ret;
	}
	// Try to save the situation at least somewhat...
	return format;
}

const char* strings_vsprintf_msvcrt14(const char *format, const size_t slot, va_list va)
{
	return strings_vsprintf(slot, format, va);
}

const char* strings_sprintf(const size_t slot, const char *format, ...)
{
	va_list va;
	const char *ret;
	va_start(va, format);
	ret = strings_vsprintf(slot, format, va);
	return ret;
}

const char* strings_strclr(const size_t slot)
{
	char *ret = strings_storage_get(slot, 0);
	if(ret) {
		ret[0] = 0;
	}
	return ret;
}

const char* strings_strcat(const size_t slot, const char *src)
{
	char *ret = strings_storage_get(slot, 0);
	size_t ret_len = strlen(ret);
	size_t src_len;

	src = strings_lookup(src, &src_len);

	ret = strings_storage_get(slot, ret_len + src_len + 1);
	if(ret) {
		strncpy(ret + ret_len, src, src_len + 1);
		return ret;
	}
	// Try to save the situation at least somewhat...
	return src;
}

const char* strings_replace(const size_t slot, const char *src, const char *dst)
{
	char *ret = strings_storage_get(slot, 0);
	dst = dst ? dst : "";
	if(src && ret) {
		size_t src_len = strlen(src);
		size_t dst_len = strlen(dst);
		while(ret) {
			char *src_pos = NULL;
			char *copy_pos = NULL;
			char *rest_pos = NULL;
			size_t ret_len = strlen(ret);
			// We do this first since the string address might change after
			// reallocation, thus invalidating the strstr() result
			ret = strings_storage_get(slot, ret_len + dst_len);
			if(!ret) {
				break;
			}
			src_pos = strstr(ret, src);
			if(!src_pos) {
				break;
			}
			copy_pos = src_pos + dst_len;
			rest_pos = src_pos + src_len;
			memmove(copy_pos, rest_pos, strlen(rest_pos) + 1);
			memcpy(src_pos, dst, dst_len);
		}
	}
	// Try to save the situation at least somewhat...
	return ret ? ret : dst;
}

/// String lookup hooks
/// -------------------
int WINAPI strings_MessageBoxA(
	HWND hWnd,
	LPCSTR lpText,
	LPCSTR lpCaption,
	UINT uType
)
{
	lpText = strings_lookup(lpText, NULL);
	lpCaption = strings_lookup(lpCaption, NULL);
	return chain_MessageBoxU(hWnd, lpText, lpCaption, uType);
}

HANDLE WINAPI strings_FindFirstFileA(
	LPCSTR lpFileName,
	LPWIN32_FIND_DATAA lpFindFileData
)
{
	return chain_FindFirstFileU(strings_lookup(lpFileName, NULL), lpFindFileData);
}

HANDLE WINAPI strings_CreateFileA(
	LPCSTR                lpFileName,
	DWORD                 dwDesiredAccess,
	DWORD                 dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD                 dwCreationDisposition,
	DWORD                 dwFlagsAndAttributes,
	HANDLE                hTemplateFile
)
{
	return chain_CreateFileU(
		strings_lookup(lpFileName, NULL),
		dwDesiredAccess,
		dwShareMode,
		lpSecurityAttributes,
		dwCreationDisposition,
		dwFlagsAndAttributes,
		hTemplateFile
	);
}
/// -------------------

void strings_mod_init(void)
{
	jsondata_add("stringdefs.js");
	stringlocs_reparse();
}

void strings_mod_detour(void)
{
	detour_chain("user32.dll", 1,
		"MessageBoxA", strings_MessageBoxA, &chain_MessageBoxU,
		NULL
	);
	detour_chain("kernel32.dll", 2,
		"FindFirstFileA", strings_FindFirstFileA, &chain_FindFirstFileU,
		"CreateFileA", strings_CreateFileA, &chain_CreateFileU,
		NULL
	);
}

void strings_mod_repatch(json_t *files_changed)
{
	const char *key;
	const json_t *val;
	json_object_foreach(files_changed, key, val) {
		if(strstr(key, "/stringlocs.")) {
			stringlocs_reparse();
		}
	}
}

void strings_mod_exit(void)
{
	for(auto& i : strings_storage) {
		SAFE_FREE(i.second);
	}
	strings_storage.clear();
	stringlocs.clear();
	backing_obj = json_decref_safe(backing_obj);
}
