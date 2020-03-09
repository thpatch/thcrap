#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// TODO: file write error callback
int RepoDiscoverAtURL(const char *start_url/*, file_write_error_t *fwe_callback*/);
int RepoDiscoverFromLocal(/*file_write_error_t *fwe_callback*/);

#ifdef __cplusplus
}
#endif
