#pragma once

#include <atomic>
#include <filesystem>
#include <list>
#include <mutex>
#include <vector>
#include "download_cache.h"
#include "download_url.h"
#include "http_interface.h"

class File
{
public:
    typedef std::function<void (const DownloadUrl& url, std::vector<uint8_t>& data)> success_t;
    typedef std::function<void (const DownloadUrl& url, HttpStatus status)> failure_t;
    typedef std::function<bool (const DownloadUrl& url, size_t file_progress, size_t file_size)> progress_t;
    static void defaultSuccessFunction(const DownloadUrl&, std::vector<uint8_t>&) {}
    static void defaultFailureFunction(const DownloadUrl&, HttpStatus) {}
    static bool defaultProgressFunction(const DownloadUrl&, size_t, size_t) { return true; }

private:
    enum class Status
    {
        Todo,
        Downloading,
        Done,
    };

    std::atomic<Status> status;
    // List of URLs we can download this file from.
    // The end of the URL should always be the same, and the beginning should
    // always be different - the different URLs point to the same file on
    // different servers.
    // When starting a download, we remove the corresponding URL from the list.
    std::list<DownloadUrl> urls;
	DownloadCache *cache;

    // User-provided callbacks
    success_t userSuccessCallback;
    failure_t userFailureCallback;
    progress_t userProgressCallback;

    size_t writeCallback(std::vector<uint8_t>& buffer, const uint8_t *data, size_t size);
    bool progressCallback(const DownloadUrl& url, size_t dlnow, size_t dltotal);
    DownloadUrl pickUrl();
    void download(IHttpHandle& http, const DownloadUrl& url);

public:
    File(std::list<DownloadUrl>&& urls,
         success_t successCallback = defaultSuccessFunction,
         failure_t failureCallback = defaultFailureFunction,
         progress_t progressCallback = defaultProgressFunction,
		 DownloadCache *cache = nullptr);
    File(const File&) = delete;
    File(File&&) = delete;
    File& operator=(const File&) = delete;
    File& operator=(File&&) = delete;
	~File();

    // Start a download thread.
    // The status of the download will be returned in the callbacks given
    // in the constructor.
    void download();
};
