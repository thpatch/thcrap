#include <filesystem>
#include <fstream>
#include <map>
#include <jansson.h>
#include "netcode.h"
#include "downloader.h"
#include "server.h"

int main()
{
    // TODO: move into thcrap
    curl_global_init(CURL_GLOBAL_DEFAULT);

    //RepoDiscoverAtURL("https://mirrors.thpatch.net/nmlgc/"); // TODO: why is it the default??
    //if (RepoDiscoverAtURL("https://srv.thpatch.net/") != 0) {
    //    // TODO: log_printf (on every file)
    //    printf("Discovery from URL failed\n");
    //}
    //if (RepoDiscoverFromLocal() != 0) {
    //    printf("Discovery from local failed\n");
    //}

    json_t *lang_en_patch = ServerCache::get().downloadJsonFile("https://srv.thpatch.net/lang_en/patch.js");
    json_t *lang_en_files = ServerCache::get().downloadJsonFile("https://srv.thpatch.net/lang_en/files.js");

    std::list<std::string> servers;
    std::map<std::string, const File*> files;
    Downloader downloader;
    size_t i;
    const char *key;
    json_t *value;
    json_array_foreach(json_object_get(lang_en_patch, "servers"), i, value) {
        servers.push_back(json_string_value(value));
    }
    json_object_foreach(lang_en_files, key, value) {
        // TODO: it would be nice if addFile could take a lambda as parameter.
        // Or if its return value could work like a javascript-style promise
        // (maybe the C++ promises allow the same coding style, I need to check).
        // TODO: use CRC32
        files[key] = downloader.addFile(servers, key);
    }
    downloader.wait();
    for (auto& it : files) {
        if (it.second->getStatus() != FileStatus::DONE) {
            continue;
        }
        std::filesystem::path path = "lang_en/" + it.first;
        std::filesystem::path dir = path;
        dir.remove_filename();
        std::filesystem::create_directories(dir);
        std::ofstream f(path);
        const auto& data = it.second->getData();
        f.write(reinterpret_cast<const char*>(data.data()), data.size());
    }
}
