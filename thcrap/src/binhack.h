/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Binary hack handling.
  *
  * Binary hacks consist of a "code" string that is rendered at one or more
  * addresses, optionally if the previous bytes at those addresses match an
  * "expected" byte sequence.
  *
  * "code" supports the following data types, which can occur anywhere in the
  * string:
  * • Lower- or uppercase hex characters (0-9, a-f, A-F), describing bytes.
  *   A single byte is always made up of two adjacent hex characters.
  * • [Relative] or <absolute> pointers to named functions exported by any
  *   thcrap module
  * • float and double literals in the typical C syntax, which are rendered
  *   into their 4- (if postfixed with f or F, as in C) or 8-byte IEEE 754
  *   representation. However, specifying the +/- sign in front is
  *   *mandatory*, to keep parsing sane. Also, these literals must be
  *   delimited with spaces, or come at the beginning or end of the "code"
  *   string.
  * • Anything else not matching these types is ignored.
  *
  * They can also be disabled by setting "ignore" to true.
  */

#pragma once

typedef struct {
	// Binhack name
	char *name;
	// Binhack description
	char *title;
	// Binhack code
	char *code;
	// Expected code at the binhack address
	char *expected;
	// Binhack addresses (NULL-terminated array)
	// They are passed as strings and resolved by binhacks_apply.
	char **addr;
} binhack_t;

typedef enum {
	NOACCESS = 0,
	READONLY = 1,
	WRITECOPY = 2,
	READWRITE = 3,
	EXECUTE = 4,
	EXECUTE_READ = 5,
	EXECUTE_WRITECOPY = 6,
	EXECUTE_READWRITE = 7
} CodecaveAccessType;

typedef struct {
	// Codecave name
	char *name;
	// Codecave code
	char *code;
	// Codecave size
	size_t size;
	// Codecave count
	size_t count;
	// Codecave fill
	BYTE fill;
	// Read, write, execute flags
	CodecaveAccessType access_type;
} codecave_t;

// Returns whether [c] is a valid hexadecimal character
int is_valid_hex(char c);

// Shared error message for nonexistent functions.
int hackpoints_error_function_not_found(const char *func_name, int retval);

// Parses a json binhack entry and returns a binhack object
bool binhack_from_json(const char *name, json_t *in, binhack_t *out);

// Calculate the rendered length in bytes of [binhack_str], the JSON representation of a binary hack
size_t binhack_calc_size(const char *binhack_str);

// Renders [binhack_str], the JSON representation of a binary hack, into the byte array [binhack_buf].
// [target_addr] is used as the basis to resolve relative pointers.
int binhack_render(BYTE *binhack_buf, size_t target_addr, const char *binhack_str);

// Applies every binary hack in [binhacks] irreversibly on the current process.
// If HMODULE is not null, relative addresses are relative to this module.
// Else, they are relative to the main module of the current process.
// Returns the number of binary hacks that could not be applied.
int binhacks_apply(const binhack_t *binhacks, size_t binhacks_count, HMODULE hMod);


// Adds every codecave in [codecaves] on the current process.
// The codecaves can then be called from binhacks similar to plugin functions. For example:
// "binhacks": {
//		"test_hack": {
//			"code": "e9[codecave:test_cave]"
//		}
// }
// "codecaves": {
//		"test_cave": "somecode"
//		}
// }
int codecaves_apply(const codecave_t *codecaves, size_t codecaves_count);

// Parses a json codecave entry and returns a codecave object
bool codecave_from_json(const char *name, json_t *in, codecave_t *out);
