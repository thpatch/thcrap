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

class File
{
public:
    typedef std::function<void (const DownloadUrl& url, std::vector<uint8_t>& data)> success_t;
    typedef std::function<void (const DownloadUrl& url, HttpHandle::Status status)> failure_t;
    typedef std::function<bool (const DownloadUrl& url, size_t file_progress, size_t file_size)> progress_t;
    static void defaultSuccessFunction(const DownloadUrl&, std::vector<uint8_t>&) {}
    static void defaultFailureFunction(const DownloadUrl& url, HttpHandle::Status status) {}
    static bool defaultProgressFunction(const DownloadUrl& url, size_t file_progress, size_t file_size) { return true; }

private:
    enum class Status
    {
        Todo,
        Downloading,
        Done,
    };

    std::mutex mutex;
    std::atomic<Status> status;
    // threadsLeft is the number of threads left to enqueue into the thread pool.
    // It is tightly coupled with urls.size(), but may be lower.
    // threadsLeft will decrease when we enqueue a download into the thread pool,
    // but urls.size() will decrease only when the thread pool picks our download
    // and starts it.
    std::list<DownloadUrl> urls;
    size_t threadsLeft;

    // User-provided callbacks
    success_t userSuccessCallback;
    failure_t userFailureCallback;
    progress_t userProgressCallback;

    size_t writeCallback(std::vector<uint8_t>& buffer, const uint8_t *data, size_t size);
    bool progressCallback(const DownloadUrl& url, size_t dlnow, size_t dltotal);
    DownloadUrl pickUrl();
    void download(HttpHandle& http, const DownloadUrl& url);

public:
    File(std::list<DownloadUrl>&& urls,
         success_t successCallback = defaultSuccessFunction,
         failure_t failureCallback = defaultFailureFunction,
         progress_t progressCallback = defaultProgressFunction);
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    // Used by the Downloader class
    size_t getThreadsLeft() const;
    void decrementThreadsLeft();

    // Start a download thread.
    // The status of the download will be returned in the callbacks given
    // in the constructor.
    void download();
};
