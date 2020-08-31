#pragma once

#include <list>
#include <string>
#include <jansson.h>
#include "downloader.h"

#include "thcrap.h"

typedef enum {
    // Download in progress. file_size may be 0 if it isn't known yet.
    GET_DOWNLOADING,
    // Download completed successfully
    GET_OK,
    // Error with the file (4XX error codes)
    GET_CLIENT_ERROR, // 404, 502, etc.
    // Error with the server (timeout, 5XX error code etc)
    GET_SERVER_ERROR,
    // Download cancelled. You will see this if you return
    // false to the progress callback, or if we tried to
    // download a file from 2 different URLs at the same time.
    GET_CANCELLED,
    // Internal error in the download library or when
    // writing the file
    GET_SYSTEM_ERROR,
} get_status_t;

typedef struct {
    // Patch containing the file in download
    const patch_t *patch;
    // File name
    const char *fn;
    // Download URL
    const char *url;
    // File download status or result
    get_status_t status;

    // Bytes downloaded for the current file
    size_t file_progress;
    // Size of the current file
    size_t file_size;

    // Number of files downloaded in the current session
    size_t nb_files_downloaded;
    // Number of files to download. Note that it will be 0 if
    // all the files.js haven't been downloaded yet.
    size_t nb_files_total;
} progress_callback_status_t;

// Callback called when the download progresses.
// This callback will always be called at least twice per file:
// - A first time with GET_DOWNLOADING, file_progress=0 and file_size=0
// - Between 0 and many times with GET_DOWNLOADING
// - A last time with either GET_OK or a GET_ error code.
// Note that several files from several patches can be downloaded
// at the same time. Never assume 2 consecutive calls to the callback
// have the same patch or fn parameter.
// If status is GET_START or GET_DOWNLOADING, this callbacks should
// return true if the download is allowed to continue, or false to cancel it.
// If status is GET_OK or a GET_ error code, the return value is ignored.
typedef bool (*progress_callback_t)(progress_callback_status_t *status, void *param);

typedef int (*update_filter_func_t)(const char *fn, void *filter_data);

// Returns 1 for all global file names, i.e. those without a slash.
int update_filter_global(const char *fn, void*);
// Returns 1 for all global file names and those that are specific to a game
// in the strings array [games].
int update_filter_games(const char *fn, void *games);

class Update
{
private:
    typedef std::function<bool(const std::string&)> filter_t;

    // Downloader used for the files.js downloads
    Downloader filesJsDownloader;
    // Downloader used for every other files.
    // We need a separate downloader because the wait() function tells to the downloader
    // the files list is complete. And the patches files list is complete when every
    // files.js is downloaded.
    Downloader mainDownloader;

    filter_t filterCallback;
    progress_callback_t progressCallback;
    void *progressData;

    void startPatchUpdate(const patch_t *patch);
    void onFilesJsComplete(const patch_t *patch, const std::vector<uint8_t>& file);
    bool callProgressCallback(const patch_t *patch, const std::string& fn, const DownloadUrl& url, get_status_t getStatus,
                              size_t file_progress, size_t file_size);
    get_status_t httpStatusToGetStatus(HttpHandle::Status status);

public:
    Update(filter_t filterCallback, progress_callback_t progressCallback, void *progressData);
    void run(const std::list<const patch_t*>& patchs);
};

void stack_update(update_filter_func_t filter_func, void *filter_data, progress_callback_t progress_callback, void *progress_param);
void global_update(progress_callback_t progress_callback, void *progress_param);
