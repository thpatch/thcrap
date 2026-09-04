#pragma once

#include <list>
#include <string>
#include <vector>
#include <atomic>
#include <libs/include/ThreadPool.h>
#include "file.h"

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
                 File::SuccessCallback successCallback = File::defaultSuccessFunction,
                 File::FailureCallback failureCallback = File::defaultFailureFunction,
                 File::ProgressCallback progressCallback = File::defaultProgressFunction);
    void addFile(char** servers, std::string filename,
                 File::SuccessCallback successCallback = File::defaultSuccessFunction,
                 File::FailureCallback failureCallback = File::defaultFailureFunction,
                 File::ProgressCallback progressCallback = File::defaultProgressFunction);
    size_t current() const;
    size_t total() const;
    void wait();
};
