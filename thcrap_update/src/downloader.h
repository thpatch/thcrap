#pragma once

#include <list>
#include <string>
#include <vector>
#include <atomic>
#include <libs/include/ThreadPool.h>
#include "file.h"
#include "download_cache.h"

class Downloader
{
private:
    ThreadPool pool;
    std::vector<std::future<void>> futuresList;
    std::mutex mutex;
    std::atomic<size_t> current_;
    size_t total_;

    std::list<DownloadUrl> serversListToDownloadUrlList(const std::list<std::string>& serversUrl, const std::string& filePath);

public:
    Downloader();
    ~Downloader();
    void addFile(const std::list<std::string>& servers, std::string filename,
                 File::success_t successCallback = File::defaultSuccessFunction,
                 File::failure_t failureCallback = File::defaultFailureFunction,
                 File::progress_t progressCallback = File::defaultProgressFunction,
				 DownloadCache *cache = nullptr);
    void addFile(char** servers, std::string filename,
                 File::success_t successCallback = File::defaultSuccessFunction,
                 File::failure_t failureCallback = File::defaultFailureFunction,
                 File::progress_t progressCallback = File::defaultProgressFunction,
				 DownloadCache *cache = nullptr);
    size_t current() const;
    size_t total() const;
    void wait();
};
