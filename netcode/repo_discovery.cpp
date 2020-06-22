#include "thcrap.h"
#include <filesystem>
#include <functional>
#include <thread>
#include "server.h"
#include "repo_discovery.h"

RepoDiscover::RepoDiscover()
{}

RepoDiscover::~RepoDiscover()
{
    this->wait();
}

bool RepoDiscover::writeRepoFile(json_t *repo_js)
{
    const char *id = json_string_value(json_object_get(repo_js, "id"));
    if (!id) {
        printf("Repository file does not specify an ID!\n");
        return true;
    }

    std::string repo_fn_local = RepoGetLocalFN(id);
    std::filesystem::create_directories(std::filesystem::u8path(repo_fn_local).remove_filename());
    return json_dump_file(repo_js, repo_fn_local.c_str(), JSON_INDENT(4)) == 0;
}

void RepoDiscover::discoverNeighbors(json_t *repo_js)
{
    json_t *neighbors = json_object_get(repo_js, "neighbors");
    size_t i;
    json_t *neighbor;
    json_array_foreach(neighbors, i, neighbor) {
        const char *neighborUrl = json_string_value(neighbor);
        this->addServer(neighborUrl);
    }
}

void RepoDiscover::addServer(std::string url)
{
    std::scoped_lock<std::mutex> lock(this->mutex);

    if (url.back() != '/') {
        url += "/";
    }
    if (this->downloading.count(url) > 0 ||
        this->done.count(url) > 0) {
        return ;
    }
    // TODO: abort download if repo.js already exists. Or not. See the comments in thcrap_configure and think about it.

    this->downloading.insert(url);
    std::thread([this, url]() {
        json_t *repo_js = ServerCache::get().downloadJsonFile(url + "repo.js");
        if (!repo_js) {
            return ;
        }

        {
            std::scoped_lock<std::mutex> lock(this->mutex);
            this->downloading.erase(url);
            this->done.insert(url);
        }

        if (this->writeRepoFile(repo_js)) {
            this->discoverNeighbors(repo_js);
        }
        else {
            this->success = false;
        }

        json_decref(repo_js);
        this->condVar.notify_one();
    }).detach();
}

bool RepoDiscover::wait()
{
    // TODO: this blocks if any download faild. investigate.
    // TODO: the start_url is downloaded twice
    // TODO: if a file fails on a server, the server is thought as dead and every
    // other download is aborted. This is fine for patch download, not for repo discovery.
    // Actually, even on patch download, we don't want to invalidate the whole
    // Github servers for only one failing patch.
    std::unique_lock<std::mutex> lock(this->mutex);
    while (!this->downloading.empty()) {
        this->condVar.wait(lock);
    }
    return this->success;
}

int RepoDiscoverAtURL(const char *start_url)
{
    RepoDiscover discover;
    discover.addServer(start_url);
    return discover.wait() ? 0 : 1;
}

int RepoDiscoverFromLocal()
{
    RepoDiscover discover;

    repo_t **repo_list = RepoLoad();
    for (size_t i = 0; repo_list[i]; i++) {
        for (size_t j = 0; repo_list[i]->servers && repo_list[i]->servers[j]; j++) {
            discover.addServer(repo_list[i]->servers[j]);
        }
        RepoFree(repo_list[i]);
    }
    free(repo_list);

    return discover.wait() ? 0 : 1;
}
