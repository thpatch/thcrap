#include <condition_variable>
#include <filesystem>
#include <functional>
#include <set>
#include "netcode.h"

// TODO: move these 2 functions to thcrap/src/repo.cpp
std::string RepoGetLocalFN(const char *id)
{
    const char *repo_fn = "repo.js";
    return std::string("repos/") + id + "/" + repo_fn;
}

void repo_foreach(std::function<void(json_t*)> callback)
{
    for (auto& it : std::filesystem::directory_iterator("repos/")) {
        if (!it.is_directory()) {
            continue;
        }
        std::string repo_local_fn = RepoGetLocalFN(it.path().filename().u8string().c_str());
        json_t *repo_js = json_load_file(repo_local_fn.c_str(), 0, nullptr);
        if (repo_js) {
            callback(repo_js);
        }
        json_decref(repo_js);
    }
}

class RepoDiscover
{
private:
    std::condition_variable condVar;
    std::mutex mutex;
    std::set<std::string> downloading;
    std::set<std::string> done;
    bool success = true;

    bool writeRepoFile(json_t *repo_js);

public:
    RepoDiscover();
    ~RepoDiscover();

    // Start a discovery from url.
    // The discovery runs on another thread.
    // If this server is already known, do nothing.
    void addServer(const std::string& url);
    // Start a discovery for every neighbor in repo.
    void discoverNeighbors(json_t *repo_js);
    // Wait until all running discoveries are finished.
    // Return false if a repo couldn't be written because of a file write error,
    // true otherwise. It is not an error if a server can't be accessed.
    bool wait();
};

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

void RepoDiscover::addServer(const std::string& url)
{
    std::scoped_lock<std::mutex> lock(this->mutex);

    if (this->downloading.count(url) > 0 ||
        this->done.count(url) > 0) {
        return ;
    }
    // TODO: abort download if repo.js already exists. Or not. See the comments in thcrap_configure and think about it.

    this->downloading.insert(url);
    std::thread([this, url]() {
        json_t *repo_js = Server(url).downloadJsonFile("repo.js");
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

    repo_foreach([&discover](json_t *repo) {
        json_t *servers = json_object_get(repo, "servers");
        // TODO: support several servers
        discover.addServer(json_string_value(json_array_get(servers, 0)));
    });
    return discover.wait() ? 0 : 1;
}
