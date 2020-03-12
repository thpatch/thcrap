#include "downloader.h"
#include "server.h"

Downloader::Downloader()
    : pool(8)
{}

Downloader::~Downloader()
{
    this->wait();
}

std::list<DownloadUrl> Downloader::serversListToDownloadUrlList(const std::list<std::string>& serversUrl, const std::string& filePath)
{
    std::list<DownloadUrl> urls;
    std::transform(serversUrl.begin(), serversUrl.end(), std::back_inserter(urls), [&filePath](const std::string& url) {
        auto [server, urlPath] = ServerCache::get().urlToServer(url + filePath);
        return DownloadUrl { .server = server, .url = urlPath };
    });
    return urls;
}

const File* Downloader::addFile(const std::list<std::string>& serversUrl, std::string filePath, Downloader::callback_t successCallback)
{
    std::list<DownloadUrl> urls = this->serversListToDownloadUrlList(serversUrl, filePath);
    this->files.emplace_back(std::move(urls), successCallback);
    auto& file = this->files.back();
    this->addToQueue(file);
    return &file.first;
}

void Downloader::addToQueue(std::pair<File, callback_t>& file)
{
    file.first.decrementThreadsLeft();
    this->futuresList.push_back(this->pool.enqueue([&file]() {
        if (file.first.download()) {
            // TODO: fail somewhere if this returns false
            file.second(file.first);
        }
    }));
}

void Downloader::wait()
{
    // For every file, run as many download threads as there is servers available.
    // The 2nd thread for a file will run only after every file have started once.
    bool loop = false;
    do {
        loop = false;
        for (auto& it : this->files) {
            File& file = it.first;
            if (file.getThreadsLeft() > 0) {
                this->addToQueue(it);
            }
            if (file.getThreadsLeft() > 0) {
                loop = true;
            }
        }
    } while (loop == true);

    for (auto& it : this->futuresList) {
        it.get();
    }
    this->futuresList.clear();
}

// TODO: move some of this stuff to another file
struct Patch
{
    std::string path;
    std::string name;
    std::list<std::string> servers;
};

class ScopedJson
{
private:
    json_t *obj;

public:
    ScopedJson(json_t *obj)
        : obj(obj)
    {}
    ScopedJson(const ScopedJson& src) = delete;
    ScopedJson(ScopedJson&& src) = delete;
    ScopedJson& operator=(const ScopedJson& src) = delete;
    ScopedJson& operator=(ScopedJson&& src) = delete;
    ~ScopedJson()
    {
        json_decref(obj);
    }
    void operator()(json_t *obj)
    {
        json_decref(obj);
    }
    json_t *operator*()
    {
        return this->obj;
    }
};

static Patch initPatch(const std::string& path)
{
    ScopedJson patch = json_load_file((path + "/patch.js").c_str(), 0, nullptr);
    if (*patch == nullptr) {
        throw std::runtime_error("File not found");
    }

    Patch patchObj {
        .path = path,
        .name = json_string_value(json_object_get(*patch, "id")),
    };
    size_t i;
    json_t *value;
    json_array_foreach(json_object_get(*patch, "servers"), i, value) {
        patchObj.servers.push_back(json_string_value(value));
    }

    return patchObj;
}

bool patches_update(const std::list<std::string>& patchPaths)
{
    std::list<Patch> patches;
    Downloader dlFilesJs;
    Downloader dlPatchFiles;

    for (const std::string& path : patchPaths) {
        std::list<std::string> servers;
        try {
            patches.push_back(initPatch(path));
        }
        catch (std::runtime_error&) {
            printf("Could not load %s\n", (path + "/patch.js").c_str());
            return false;
        }
    }

    for (const Patch& patch : patches) {
        dlFilesJs.addFile(patch.servers, "files.js", [&patch = std::as_const(patch), &dlPatchFiles](const File& file) {
            file.write(patch.path + "/files.js");

            ScopedJson filesJs = json_loadb(reinterpret_cast<const char*>(file.getData().data()), file.getData().size(), 0, nullptr);
            if (*filesJs == nullptr) {
                printf("%s: files.js isn't a valid json file!\n", patch.name.c_str());
                return false;
            }

            const char *key;
            json_t *value;
            json_object_foreach(*filesJs, key, value) {
                if (json_is_null(value)) {
                    continue;
                }

                // TODO: check CRC32
                dlPatchFiles.addFile(patch.servers, key, [&patch, key = std::string(key)](const File& file) {
                    file.write(patch.path + "/" + key);
                    return true;
                });
            }

            return true;
        });
    }

    dlFilesJs.wait();
    // At this point, dlPatchFiles have been fully populated
    dlPatchFiles.wait();
    return true;
}
