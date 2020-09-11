#pragma once

#include "thcrap.h"
#include <jansson.h>

#ifdef __cplusplus

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <string>

class RepoDiscovery
{
private:
    std::condition_variable condVar;
    std::mutex mutex;
    std::atomic<size_t> downloading;
    std::set<std::string> urls;
    std::map<std::string, repo_t*> repos;

public:
    RepoDiscovery();
    ~RepoDiscovery();

    // Start a discovery from an url.
    // The discovery runs on another thread.
    // If this server is already known, do nothing.
    void addServer(std::string url);
    // Add a repo to the known repos, and start a discovery
    // on its servers and neighbors.
    // The ownership of repo is transfered to RepoDiscovery.
    void addRepo(repo_t *repo);
    // Parse repo_js and call addRepo() on the resulting repo_t.
    // Return false if the json file is not a valid repo.
    bool addRepo(ScopedJson repo_js);
    // Wait until all running discoveries are finished.
    void wait();
    // Return the repos found by this discovery.
    // The ownership of the repos is transfered to the caller.
    repo_t **transferRepoList();
};

extern "C" {
#endif // __cplusplus

repo_t **RepoDiscover(const char *start_url);

#ifdef __cplusplus
}
#endif
