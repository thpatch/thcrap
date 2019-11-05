/**
  * Touhou Community Reliant Automatic Patcher
  * Update module
  *
  * ----
  *
  * Digitally signed, automatic updates of thcrap itself.
  */

#pragma once

// All possible ways a self-update can possibly fail
typedef enum {
	SELF_NO_UPDATE = -1,
	SELF_OK = 0,
	SELF_NO_PUBLIC_KEY,
	SELF_SERVER_ERROR,
	SELF_DISK_ERROR,
	SELF_NO_SIG,
	SELF_SIG_FAIL,
	SELF_REPLACE_ERROR,
	SELF_VERSION_CHECK_ERROR,
	SELF_INVALID_NETPATH,
	SELF_NO_TARGET_VERSION,
} self_result_t;

// Performs an update of the thcrap installation in [thcrap_dir].
// [arc_fn_ptr] receives the name of the archive downloaded.
self_result_t self_update(const char *thcrap_dir, char **arc_fn_ptr);

// Returns a string representation of the next version. If no update was
// attempted or if no info could be retrieved from the server it returns a blank string.
const char* self_get_target_version();
