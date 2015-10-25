/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Zipfile handling.
  */

#pragma once

// TODO: This shouldn't be publicly visible
typedef struct {
	json_t *files;
	json_t *files_empty;
	HANDLE hArc;
	BYTE *cmt;
	size_t cmt_len;
} zip_t;

// Returns a JSON object that maps the names of all non-empty files in [zip]
// to the position of their file header.
json_t* zip_list(zip_t *zip);

// Returns a JSON array containing the names of all empty files in [zip].
json_t* zip_list_empty(zip_t *zip);

// Returns the archive comment.
const BYTE* zip_comment(zip_t *zip, size_t *cmt_len);

// Unzips [fn] in [zip] to a newly created buffer and returns its file size in
// [file_size]. Return value has to be free()d by the caller!
void* zip_file_load(zip_t *zip, const char *fn, size_t *file_size);

// Unzips [fn] in [zip] to the current directory while preserving the original
// timestamp.
int zip_file_unzip(zip_t *zip, const char *fn);

zip_t* zip_open(const char *fn);
zip_t* zip_close(zip_t *zip);
