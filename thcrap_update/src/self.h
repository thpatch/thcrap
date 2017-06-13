/**
  * Touhou Community Reliant Automatic Patcher
  * Update plugin
  *
  * ----
  *
  * Digitally signed, automatic updates of thcrap itself.
  */

#pragma once

// All possible ways a self-update can possibly fail
typedef enum {
	SELF_OK = 0,
	SELF_NO_PUBLIC_KEY,
	SELF_SERVER_ERROR,
	SELF_DISK_ERROR,
	SELF_NO_SIG,
	SELF_SIG_FAIL,
	SELF_REPLACE_ERROR,
} self_result_t;

// Performs an update of the thcrap installation in [thcrap_dir].
// [arc_fn_ptr] receives the name of the archive downloaded.
self_result_t self_update(const char *thcrap_dir, char **arc_fn_ptr);
