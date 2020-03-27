#pragma once

#include <atomic>
#include <filesystem>
#include <list>
#include <mutex>
#include <vector>
#include "download_url.h"

#ifdef USE_HTTP_CURL
# include "http_curl.h"
#elif defined(USE_HTTP_WININET)
# include "http_wininet.h"
#else
# error "Unknown http library. Please define either USE_HTTP_CURL or USE_HTTP_WININET"
#endif

enum class FileStatus
{
    TODO,
    DOWNLOADING,
    DONE,
    FAILED
};

class File
{
private:
    std::mutex mutex;
    std::vector<uint8_t> data;
    std::atomic<FileStatus> status;
    // threadsLeft is the number of threads left to enqueue into the thread pool.
    // It is tightly coupled with urls.size(), but may be lower.
    // threadsLeft will decrease when we enqueue a download into the thread pool,
    // but urls.size() will decrease only when the thread pool picks our download
    // and starts it.
    std::list<DownloadUrl> urls;
    size_t threadsLeft;

    size_t writeCallback(std::vector<uint8_t>& buffer, const uint8_t *data, size_t size);
    bool progressCallback(const DownloadUrl& url, size_t dlnow, size_t dltotal);
    bool setDownloading();
    void setFailed(const DownloadUrl& url);
    DownloadUrl pickUrl();
    bool download(HttpHandle& http, const DownloadUrl& url);

public:
    File(std::list<DownloadUrl>&& urls);
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    const std::vector<uint8_t>& getData() const;
    FileStatus getStatus() const;
    size_t getThreadsLeft() const;
    void decrementThreadsLeft();

    // True if the file have been downloaded by this thread, false otherwise.
    // Note that if the file have been successfully downloaded by another thread,
    // this function still returns false (the rationale is that the caller will call
    // a completion callback if it returns true. If the completion callback have already
    // be run by another thread, we don't want to run it again. Use getStatus() after
    // all the download threads are finished to get the real status.
    bool download();
    // Write the file to the disk.
    // If any directory in the path doesn't exist, it is created.
    // Throws an exception on error.
    void write(const std::filesystem::path& path) const;
};
