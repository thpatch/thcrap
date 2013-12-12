/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Binary hack handling.
  */

#pragma once

// Returns whether [c] is a valid hexadecimal character
int is_valid_hex(char c);

// Calculate the rendered length in bytes of [binhack_str], the JSON representation of a binary hack
size_t binhack_calc_size(const char *binhack_str);

// Renders [binhack_str], the JSON representation of a binary hack, into the byte array [binhack_buf].
// Valid function references are resolved using the pointers in the JSON object [funcs].
// [target_addr] is used as the basis to resolve relative pointers.

// Format of [funcs]:
// {
//	"<function name>": <pointer as integer>
// }
int binhack_render(char *binhack_buf, size_t target_addr, const char *binhack_str, json_t *funcs);

// Applies every binary hack in [binhacks] irreversibly on the current process.
// Function names are resolved using the pointers in [funcs].
int binhacks_apply(json_t *binhacks, json_t *funcs);
