/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Hackpoint handling.
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

// TODO: Test whether a different default size is more efficient.
#define BINHACK_BUFSIZE_MIN 512

typedef enum {
	INVALID_ADDR = -1,
	END_ADDR = 0,
	STR_ADDR = 1,
	RAW_ADDR = 2
} hackpoint_addr_type;

typedef struct {
	// Raw numeric address. Evaluation results of string addresses
	// are written to this field to be referenced later without
	// re-evaluating.
	uintptr_t raw;

	// Heap allocated string expression to be evaluated.
	const char* str;

	/**
	  * Internal values
	  */

	// Value indicating which type of JSON data was used to create this
	// hackpoint_addr.
	int8_t type;

	// Pointer to the data originally overwritten at this hackpoint_addr.
	// Currently implemented with a union for documentation purposes since
	// binhacks/breakpoints useincompatible methods of allocating this buffer.
	// 
	// TODO: In theory, this should be enough to implement binhack/breakpoint
	// repatching by iterating through the runconfig stages in reverse and copying
	// the buffers back to the original program.
	union {
		uint8_t* breakpoint_source;
		uint8_t* binhack_source;
	};
} hackpoint_addr_t;

// Parses a JSON array of string/integer addresses and returns an array
// of hackpoint_addr_t to be parsed later by eval_hackpoint_addr.
TH_CALLER_DELETEA hackpoint_addr_t* hackpoint_addrs_from_json(json_t* addr_array);

// Evaluates a [hackpoint_addr], potentially converting the contained string expression into an
// integer. If HMODULE is not null, relative addresses are relative to this module.
// Else, they are relative to the main module of the current process.
bool eval_hackpoint_addr(hackpoint_addr_t* hackpoint_addr, uintptr_t* out, HMODULE hMod);

typedef struct {
	// Binhack name
	const char *name;
	// Binhack description
	const char *title;
	// Binhack code
	const char *code;
	// Expected code at the binhack address
	const char *expected;
	// Binhack addresses (NULL-terminated array)
	// They are passed as strings and resolved by binhacks_apply.
	hackpoint_addr_t* addr;
} binhack_t;

enum {
	READONLY = 0,
	READWRITE = 1,
	EXECUTE = 2,
	EXECUTE_READ = 3,
	EXECUTE_READWRITE = 4
};
typedef uint8_t CodecaveAccessType;

typedef struct {
	// Codecave name
	const char *name;
	// Codecave code
	const char *code;
	// Codecave size
	size_t size;
	// Codecave fill
	uint8_t fill;
	// Codecave export status
	bool export_codecave;
	// Read, write, execute flags
	CodecaveAccessType access_type;
} codecave_t;

// Shared error message for nonexistent functions.
void hackpoints_error_function_not_found(const char *func_name);

// Calculate the length in bytes of [code_str], the string representation of a binary hack
size_t code_string_calc_size(const char* code_str);

// Renders [code_str], the string representation of a binary hack,
// into the byte array [output_buffer]. [target_addr] is used as
// the basis to resolve relative patch values and wildcard bytes.
int code_string_render(uint8_t* output_buffer, uintptr_t target_addr, const char* code_str);

// Parses a json binhack entry and returns a binhack object
bool binhack_from_json(const char* name, json_t* in, binhack_t* out);

// Applies every binary hack in [binhacks] irreversibly on the current process.
// If HMODULE is not null, relative addresses are relative to this module.
// Else, they are relative to the main module of the current process.
// Returns the number of binary hacks that could not be applied.
size_t binhacks_apply(const binhack_t *binhacks, size_t binhacks_count, HMODULE hMod);

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
size_t codecaves_apply(const codecave_t *codecaves, size_t codecaves_count);

// Parses a json codecave entry and returns a codecave object
bool codecave_from_json(const char *name, json_t *in, codecave_t *out);

// Compatibility definitions for old code.
// These are merely code_string_calc_size and
// code_string_render exported under different names.
TH_IMPORT size_t binhack_calc_size(const char* code_str);
TH_IMPORT int binhack_render(uint8_t* output_buffer, uintptr_t target_addr, const char* code_str);
