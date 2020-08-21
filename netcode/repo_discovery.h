#pragma once

#include "thcrap.h"
#include <jansson.h>

#ifdef __cplusplus

#include <condition_variable>
#include <mutex>
#include <set>
#include <string>

class RepoDiscover
{
private:
    std::condition_variable condVar;
    std::mutex mutex;
    std::set<std::string> downloading;
    std::set<std::string> done;
    bool success = true;

    bool writeRepoFile(ScopedJson repo_js);

public:
    RepoDiscover();
    ~RepoDiscover();

    // Start a discovery from url.
    // The discovery runs on another thread.
    // If this server is already known, do nothing.
    void addServer(std::string url);
    // Start a discovery for every neighbor in repo.
    void discoverNeighbors(ScopedJson repo_js);
    // Wait until all running discoveries are finished.
    // Return false if a repo couldn't be written because of a file write error,
    // true otherwise. It is not an error if a server can't be accessed.
    bool wait();
};

extern "C" {
#endif // __cplusplus

// TODO: file write error callback
int RepoDiscoverAtURL(const char *start_url/*, file_write_error_t *fwe_callback*/);
int RepoDiscoverFromLocal(/*file_write_error_t *fwe_callback*/);

#ifdef __cplusplus
}
#endif
