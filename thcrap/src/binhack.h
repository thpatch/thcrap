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

// Shared error message for nonexistent functions.
int hackpoints_error_function_not_found(const char *func_name, int retval);

// Calculate the rendered length in bytes of [binhack_str], the JSON representation of a binary hack
size_t binhack_calc_size(const char *binhack_str);

// Renders [binhack_str], the JSON representation of a binary hack, into the byte array [binhack_buf].
// [target_addr] is used as the basis to resolve relative pointers.
int binhack_render(BYTE *binhack_buf, size_t target_addr, const char *binhack_str);

// Returns the number of all individual instances of binary hacks or
// breakpoints in [hackpoints].
size_t hackpoints_count(json_t *hackpoints);

// Applies every binary hack in [binhacks] irreversibly on the current process.
// If HMODULE is not null, relative addresses are relative to this module.
// Else, they are relative to the main module of the current process.
int binhacks_apply(json_t *binhacks, HMODULE hMod);
