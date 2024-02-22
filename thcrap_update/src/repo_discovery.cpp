#include "thcrap.h"
#include <filesystem>
#include <functional>
#include <thread>
#include "server.h"
#include "repo_discovery.h"

RepoDiscovery::RepoDiscovery()
    : downloading(0)
{}

RepoDiscovery::~RepoDiscovery()
{
    this->wait();
    for (auto& it : this->repos) {
        RepoFree(it.second);
    }
}

void RepoDiscovery::addRepo(repo_t *repo)
{
    {
        std::scoped_lock<std::mutex> lock(this->mutex);
        if (this->repos[repo->id] != nullptr) {
            // We already know this repo
            RepoFree(repo);
            return ;
        }
        this->repos[repo->id] = repo;
        log_printf("%s\n", repo->id);
    }
    // Unlock the mutex. The addServer function will relock it.

    for (size_t i = 0; repo->servers && repo->servers[i]; i++) {
        this->addServer(repo->servers[i]);
    }
    for (size_t i = 0; repo->neighbors && repo->neighbors[i]; i++) {
        this->addServer(repo->neighbors[i]);
    }
}

bool RepoDiscovery::addRepo(ScopedJson repo_js)
{
    if (!repo_js) {
        return false;
    }

    repo_t *repo = RepoLoadJson(*repo_js);
    if (!repo) {
        return false;
    }

    this->addRepo(repo);
    return true;
}

void RepoDiscovery::addServer(std::string url)
{
    std::scoped_lock<std::mutex> lock(this->mutex);

    if (url.back() != '/') {
        url += "/";
    }
    if (this->urls.count(url) > 0) {
        // We already downloaded repo.js repo from this URL
        return ;
    }
    this->urls.insert(url);
    this->downloading++;

    std::thread([this, url]() {
        auto [repo_js, status] = ServerCache::get().downloadJsonFile(url + "repo.js");
        if (status) {
            if (!this->addRepo(repo_js)) {
                log_printf("%s: invalid repo!\n", url.c_str());
            }
        }
        else {
            log_printf("%s: %s\n", url.c_str(), status.toString().c_str());
        }

        this->downloading--;
        this->condVar.notify_one();
    }).detach();
}

void RepoDiscovery::wait()
{
    std::unique_lock<std::mutex> lock(this->mutex);
    while (this->downloading > 0) {
        this->condVar.wait(lock);
    }
}

repo_t **RepoDiscovery::transferRepoList()
{
    std::unique_lock<std::mutex> lock(this->mutex);
    repo_t **repo_list = (repo_t**)malloc((this->repos.size() + 1) * sizeof(repo_t*));

    size_t i = 0;
    for (auto& it : this->repos) {
        repo_list[i] = it.second;
        i++;
    }
    repo_list[i] = nullptr;

    this->repos.clear();
    return repo_list;
}

repo_t **RepoDiscover(const char *start_url)
{
    RepoDiscovery discover;

    // Start with the remote discovery
    if (start_url == nullptr) {
        start_url = globalconfig_get_string("discovery_start_url", DISCOVERY_DEFAULT_REPO);
    }
    log_printf("Starting repository discovery at %s...\n", start_url);
    discover.addServer(start_url);
    discover.wait();

    // If we have any local repo which isn't part of the thpatch network,
    // add it and its neighbors here.
    repo_t **repo_list = RepoLoad();
    if (repo_list) {
        for (size_t i = 0; repo_list[i]; i++) {
            discover.addRepo(repo_list[i]);
        }
        free(repo_list);
        discover.wait();
    }

    return discover.transferRepoList();
}
