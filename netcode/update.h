#pragma once

#include <list>
#include <string>
#include "downloader.h"

class Update;

class PatchUpdate
{
private:
    // Reference to the Update class. Used for downloaders, filters etc.
    Update& update;

    // Path of the patch on disk
    std::string path;
    // Patch id
    std::string id;
    // List of patch servers
    std::list<std::string> servers;

    bool onFilesJsComplete(std::shared_ptr<PatchUpdate> thisStorage, const File& file);

public:
    PatchUpdate(Update& update, std::string path, std::string id, std::list<std::string> servers);
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
    // Downloader used for the files.js downloads
    Downloader filesJsDownloader;
    // Downloader used for every other files.
    // We need a separate downloader because the wait() function tells to the downloader
    // the files list is complete. And the patches files list is complete when every
    // files.js is downloaded.
    Downloader mainDownloader;

    bool startPatchUpdate(const std::string& path);

public:
    bool run(const std::list<std::string>& patchPaths);
};

// TODO: filters
bool patches_update(const std::list<std::string>& patchPaths);
