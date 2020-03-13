#include "update.h"
#include "server.h"
#include "scoped_json.h"

PatchUpdate::PatchUpdate(Update& update, std::string path, std::string id, std::list<std::string> servers)
    : update(update), path(std::move(path)), id(std::move(id)), servers(std::move(servers))
{}

bool PatchUpdate::onFilesJsComplete(std::shared_ptr<PatchUpdate> thisStorage, const File& file)
{
    if (thisStorage.get() != this) {
        throw std::logic_error("PatchUpdate::onFilesJsComplete: this and thisStorage must be identical");
    }

    file.write(this->path + "/files.js");

    ScopedJson filesJs = json_loadb(reinterpret_cast<const char*>(file.getData().data()), file.getData().size(), 0, nullptr);
    if (*filesJs == nullptr) {
        printf("%s: files.js isn't a valid json file!\n", this->id.c_str());
        return false;
    }

    const char *key;
    json_t *value;
    json_object_foreach(*filesJs, key, value) {
        if (json_is_null(value)) {
            continue;
        }

        // TODO: check CRC32
        this->update.mainDownloader.addFile(this->servers, key, [thisStorage, key = std::string(key)](const File& file) {
            file.write(thisStorage->path + "/" + key);
            return true;
        });
    }

    return true;
}

bool PatchUpdate::start(std::shared_ptr<PatchUpdate> thisStorage)
{
    if (thisStorage.get() != this) {
        throw std::logic_error("PatchUpdate::onFilesJsComplete: this and thisStorage must be identical");
    }

    this->update.filesJsDownloader.addFile(this->servers, "files.js", [thisStorage](const File& file) {
        return thisStorage->onFilesJsComplete(thisStorage, file);
    });
    return true;
}

bool Update::startPatchUpdate(const std::string& path)
{
    ScopedJson patch = json_load_file((path + "/patch.js").c_str(), 0, nullptr);
    if (*patch == nullptr) {
        return false;
    }
    std::string name = json_string_value(json_object_get(*patch, "id"));

    std::list<std::string> servers;
    size_t i;
    json_t *value;
    json_array_foreach(json_object_get(*patch, "servers"), i, value) {
        servers.push_back(json_string_value(value));
    }

    auto patchUpdateObj = std::make_shared<PatchUpdate>(*this, path, std::move(name), std::move(servers));
    return patchUpdateObj->start(patchUpdateObj);
}

bool Update::run(const std::list<std::string>& patchPaths)
{
    bool ret;

    for (const std::string& path : patchPaths) {
        if (!this->startPatchUpdate(path)) {
            printf("Could not load %s\n", (path + "/patch.js").c_str());
            ret = false;
            // Continue and update the other patches
        }
    }

    this->filesJsDownloader.wait();
    // At this point, dlPatchFiles have been fully populated
    this->mainDownloader.wait();
    return true;
}

// TODO: remove this function. Instead, these call must work:
// stack_update_wrapper(update_filter_global_wrapper, NULL, progress_callback, NULL);
// stack_update_wrapper(update_filter_games_wrapper, filter, progress_callback, NULL);
bool patches_update(const std::list<std::string>& patchPaths)
{
    return Update().run(patchPaths);
}
