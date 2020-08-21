#pragma once

#include <list>
#include <string>
#include <jansson.h>
#include "downloader.h"

#include "thcrap.h"
typedef enum {
    GET_CANCELLED = -3,
    GET_OUT_OF_MEMORY = -2,
    GET_INVALID_PARAMETER = -1,
    GET_OK = 0,
    GET_SERVER_ERROR,
    GET_NOT_AVAILABLE, // 404, 502, etc.
} get_result_t;

// This callbacks should return TRUE if the download is allowed to continue and FALSE to cancel it.
typedef int (*progress_callback_t)(const char *patch_id, const char *fn, get_result_t ret, size_t file_progress, size_t file_total, size_t nb_files_current, size_t nb_files_total, void *param);

typedef int (*update_filter_func_t)(const char *fn, json_t *filter_data);

// Returns 1 for all global file names, i.e. those without a slash.
int update_filter_global(const char *fn, void*);
// Returns 1 for all global file names and those that are specific to a game
// in the strings array [games].
int update_filter_games(const char *fn, void *games);

class Update;

class PatchUpdate
{
private:
    // Reference to the Update class. Used for downloaders, filters etc.
    Update& update;

    // Patch object
    const patch_t *patch;
    // List of patch servers
    std::list<std::string> servers;

    bool onFilesJsComplete(std::shared_ptr<PatchUpdate> thisStorage, const File& file);

public:
    PatchUpdate(Update& update, const patch_t *path, std::list<std::string> servers);
    // We shouldn't need to copy or delete it
    PatchUpdate(const PatchUpdate& src) = delete;
    PatchUpdate(PatchUpdate&& src) = delete;
    PatchUpdate& operator=(const PatchUpdate& src) = delete;
    PatchUpdate& operator=(PatchUpdate&& src) = delete;

    // Some functions take a shared_ptr to a PatchUpdate.
    // It *must* point to this. It is used to keep this alive through the callbacks.
    bool start(std::shared_ptr<PatchUpdate> thisStorage);
};

class Update
{
friend PatchUpdate;

private:
    typedef std::function<bool(const std::string&)> filter_t;
    typedef std::function<bool(const std::string&, const std::string&, get_result_t, size_t, size_t, size_t, size_t)> progress_t;

    // Downloader used for the files.js downloads
    Downloader filesJsDownloader;
    // Downloader used for every other files.
    // We need a separate downloader because the wait() function tells to the downloader
    // the files list is complete. And the patches files list is complete when every
    // files.js is downloaded.
    Downloader mainDownloader;

    filter_t filterCallback;
    progress_t progressCallback;

    bool startPatchUpdate(const patch_t *patch);

public:
    Update(filter_t filterCallback, progress_t progressCallback);
    bool run(const std::list<const patch_t*>& patchs);
};

void stack_update(json_t *stack, update_filter_func_t filter_func, json_t *filter_data, progress_callback_t progress_callback, void *progress_param);
void global_update(progress_callback_t progress_callback, void *progress_param);
