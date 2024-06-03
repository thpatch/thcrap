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

// Length-prefixed string object used for persistent storage
typedef struct {
	char* str;
	size_t len;
	size_t buf;
} storage_string_t;

// Since we can't use the jsondata module to make this repatchable,
// we only need to keep the last parsed string around to
// provide the "backing memory" for the ID strings.
std::unordered_map<size_t, storage_string_t> strings_storage;

SRWLOCK stringlocs_srwlock = {SRWLOCK_INIT};
std::unordered_map<uintptr_t, const char *> stringlocs;

void stringlocs_reparse(void)
{
	json_t* new_obj = stack_game_json_resolve("stringlocs.js", NULL);
	const char *key;
	const json_t *val;

	AcquireSRWLockExclusive(&stringlocs_srwlock);
	stringlocs.clear();

	json_object_foreach_fast(new_obj, key, val) {
		// TODO: For now, we're nagging developers with one message box for
		// every single parse error. It'd certainly be better if we gathered
		// all errors into a single message box to be printed at the end of
		// the parsing, and even had a somewhat generic solution if we do
		// more of these conversions.
#define stringlocs_log_error(name, msg) \
	log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION, \
		"Error parsing stringlocs.js: \"%s\" " msg".", name \
	)

		if(!json_is_string(val)) {
			stringlocs_log_error(key, "must be a JSON string");
			continue;
		}
		uintptr_t addr;
		if (const char* key_end = eval_expr(key, '\0', &addr, NULL, NULL, NULL)) {
			if (key_end[0] == '\0') {
				stringlocs[addr] = json_string_value(val);
			}
			else {
				stringlocs_log_error(key, "has garbage at the end");
			}
		}
#undef stringlocs_log_error
	}

	ReleaseSRWLockExclusive(&stringlocs_srwlock);
}

const char* strings_id(const char *str)
{
	const char *ret = NULL;
	AcquireSRWLockShared(&stringlocs_srwlock);
	auto id_key = stringlocs.find((uintptr_t)str);
	if(id_key != stringlocs.end()) {
		ret = id_key->second;
	}
	ReleaseSRWLockShared(&stringlocs_srwlock);
	return ret;
}

const json_t* strings_get(const char *id)
{
	if (const json_t* stringdef_json = jsondata_get("stringdefs.js")) {
		return json_object_get(stringdef_json, id);
	}
	return NULL;
}

stringref_t strings_get_fallback(const string_named_t& sn)
{
	const json_t* ret = strings_get(sn.id);
	if(!json_is_string(ret)) {
		return sn.fallback;
	}
	return ret;
}

const char* TH_CDECL strings_lookup(const char *in, size_t *out_len)
{
	const char* str = in;
	if(str) {
		AcquireSRWLockShared(&stringlocs_srwlock);
		const json_t* stringdef_json = NULL;
		if (const char* id = strings_id(in)) {
			stringdef_json = strings_get(id);
		}
		ReleaseSRWLockShared(&stringlocs_srwlock);
		if (json_is_string(stringdef_json)) {
			str = json_string_value(stringdef_json);
		}

		if (out_len) {
			*out_len = strlen(str);
		}
	}
	return str;
}

void strings_va_lookup(va_list va, const char* format_in)
{

#ifdef PrintfFormatSafety
#define CheckFormatStr(str) if (*(str) == '\0') break; else
#else
#define CheckFormatStr(str)
#endif

	for (const char* format = format_in; *format; ++format) {
		if (*format != '%') continue;
		++format;
		CheckFormatStr(format) {
			bool is_literal_percent = *format == '%';
			format += is_literal_percent;
			if (is_literal_percent) continue;
		}
		while (
			*format == '0' ||
			*format == '#' ||
			*format == '+' ||
			*format == '-' ||
			*format == ' '
		) ++format;
		CheckFormatStr(format) {
			bool width_from_arg = *format == '*';
			format += width_from_arg;
			if (!width_from_arg) {
				while (is_valid_decimal(*format)) ++format;
			} else {
				(void)va_arg(va, int);
			}
		}
		CheckFormatStr(format) {
			bool has_precision = *format == '.';
			format += has_precision;
			if (has_precision) {
				bool precision_from_arg = *format == '*';
				format += precision_from_arg;
				if (!precision_from_arg) {
					while (is_valid_decimal(*format)) ++format;
				} else {
					(void)va_arg(va, int);
				}
			}
		}
#ifdef TH_X86
		// x64 uses 64 bits of stack for all types, so
		// everything about extra_length can be completely
		// ignored.
		bool extra_length = false;
#endif
		CheckFormatStr(format) {
			switch (*format) {
#ifdef TH_X86
				case 'L': case 'j':
					extra_length = true;
					break;
#endif
				case 'l':
#ifdef TH_X86
					extra_length = true;
#endif
				case 'h': {
					bool double_char = format[0] == format[1];
					format += double_char;
#ifdef TH_X86
					if (!double_char) extra_length = false;
#endif
					break;
				}
				case 'I': {
#ifdef PrintfFormatSafety
					if (!format[1]) break;
#endif
					uint16_t size = *(uint16_t*)&format[1];
					format += ((
#ifdef TH_X86
						extra_length =
#endif
						(size == TextInt('6', '4'))) | (size == TextInt('3', '2'))) << 1;
				}
				// case 'z': case 't': case 'w':; // default:
			}
			++format;
		}
		CheckFormatStr(format) {
			switch (*format++) {
				case 'p': case 'n': case 'Z':
				case 'c': case 'C':
#ifdef TH_X86
					extra_length = false; // x86 uses 32 bits of stack for pointers
					[[fallthrough]];
#endif
				case 'd': case 'i': case 'u': case 'x': case 'X': case 'o':
#ifdef TH_X86
					extra_length ?
						(void)va_arg(va, int64_t) :
#endif
						(void)va_arg(va, size_t); // 32 bits on x86 and 64 bits on x64
					break;
				case 's': case 'S': {
					const char*& va_str = va_arg(va, const char*);
					va_str = strings_lookup(va_str, NULL);
				}
			}
		}
	}
}

static inline storage_string_t& strings_storage_resize(storage_string_t& stored, size_t min_len)
{
	// MSVCRT's realloc implementation moves the buffer every time, even if the
	// new length is shorter. [buf] can only be 0 when the string is first created
	// since it is immediately set to 1 when null terminator is automatically appended.
	if (stored.buf <= min_len) {
		// Automatically allocate space for a null
		// terminator to simplify the math elsewhere.
		if (char* temp = (char*)realloc(stored.str, (min_len + 1))) {
			stored.buf = (min_len + 1);
			temp[min_len] = '\0';
			stored.str = temp;
		}
		else {
			// Realloc Fail
			return stored;
		}
	}
	// Record [min_len] as the new string
	// length unless realloc fails.
	stored.len = min_len;
	return stored;
}

static inline storage_string_t& strings_storage_get(const size_t slot) {
	return strings_storage.try_emplace(slot, storage_string_t{ NULL, 0, 0 }).first->second;
}

char* strings_storage_get(const size_t slot, size_t min_len)
{
	return strings_storage_resize(strings_storage_get(slot), min_len).str;
}

const char* strings_vsprintf(const size_t slot, const char *format, va_list va)
{
	format = strings_lookup(format, NULL);
	if(format) {
		va_list va2;
		va_copy(va2, va);
		strings_va_lookup(va2, format);
		va_end(va2);
		va_copy(va2, va);
		int str_len = vsnprintf(NULL, 0, format, va2);
		va_end(va2);
		storage_string_t& ret = strings_storage_get(slot);
		strings_storage_resize(ret, str_len);
		if (ret.str) {
			vsprintf(ret.str, format, va);
			return ret.str;
		}
	}
	// Fallback to [format] if nothing else works
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
	va_end(va);
	return ret;
}

const char* strings_strclr(const size_t slot)
{
	storage_string_t& ret = strings_storage_get(slot);
	ret.len = 0;
	if(ret.str) {
		ret.str[0] = '\0';
	}
	return ret.str;
}

const char* strings_strcat(const size_t slot, const char *src)
{
	storage_string_t& ret = strings_storage_get(slot);

	size_t src_length;
	src = strings_lookup(src, &src_length);

	// Save the original length for use as
	// an offset later to avoid strcat
	size_t old_length = ret.len;

	size_t total_length = src_length + old_length;
	strings_storage_resize(ret, total_length);

	// Verify that the realloc was successful
	// before copying from [src].
	if (ret.str && ret.len == total_length) {
		memcpy(ret.str + old_length, src, src_length);
		return ret.str;
	}
	else {
		// Fallback to [src] if nothing else works
		return src;
	}
}

const char* strings_replace(const size_t slot, const char *src, const char *dst)
{
	storage_string_t& ret = strings_storage_get(slot);
	dst = dst ? dst : "";
	size_t substr_len;
	if (ret.str && src && ((substr_len = strlen(src)) <= ret.len)) {
		size_t replacement_len = strlen(dst);

		// How many bytes are added/removed from the
		// total length for each substring instance.
		intptr_t length_diff = replacement_len - substr_len;

		char* parse_str = ret.str;
		while (parse_str = strstr(parse_str, src)) {

			// Convert the substring pointer into an offset
			// that can be used after a potential realloc.
			size_t substr_offset = PtrDiffStrlen(parse_str, ret.str);

			// Calculate how many bytes will need to be moved
			// to accomodate the replacement string. (+1 includes
			// the null terminator)
			size_t remaining_len = (ret.len + 1) - (substr_offset + substr_len);

			// Resize the string if necessary.
			strings_storage_resize(ret, ret.len + length_diff);

			// Use the previous offset to quickly re-find the
			// substring since that's where the replacement will
			// need to be copied to.
			char* substr_start = ret.str + substr_offset;

			// Find the end of the substring since that's where
			// all the remaining bytes will need to be moved from.
			char* after_substr = substr_start + substr_len;

			// Calcualte where to move the remaining bytes.
			// Also saves this address for continued substring
			// searching on the next iteration.
			parse_str = after_substr + length_diff;

			// Move the remaining bytes.
			memmove(parse_str, after_substr, remaining_len);

			// Copy the replacement string.
			memcpy(substr_start, dst, replacement_len);
		}
		return ret.str;
	}
	else {
		// Fallback to [dst] if nothing else works
		return dst;
	}
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

extern "C" int BP_strings_lookup(x86_reg_t * regs, json_t * bp_info)
{
	const char **string = (const char**)json_object_get_pointer(bp_info, regs, "str");
#if 0
	char *utf8str = EnsureUTF8(*string, strlen(*string));
	log_printf("[BP_strings_lookup] %p: %s\n", *string - (char*)GetModuleHandle(nullptr) , utf8str);
	free(utf8str);
#endif
	*string = strings_lookup(*string, NULL);
	return 1;
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
	json_object_foreach_key(files_changed, key) {
		if(strstr(key, "/stringlocs.")) {
			stringlocs_reparse();
			break;
		}
	}
}

void strings_mod_exit(void)
{
	for(auto& i : strings_storage) {
		SAFE_FREE(i.second.str);
	}
	strings_storage.clear();
	stringlocs.clear();
}
