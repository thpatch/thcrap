#pragma once

#include <list>
#include <string>
#include <vector>
#include "ThreadPool.h"
#include "file.h"

class Downloader
{
private:
    ThreadPool pool;
    std::list<File> files;
    std::vector<std::future<void>> futuresList;

    std::list<DownloadUrl> serversListToDownloadUrlList(const std::list<std::string>& serversUrl, const std::string& filePath);
    void addToQueue(File& file);

public:
    Downloader();
    ~Downloader();
    void addFile(const std::list<std::string>& servers, std::string filename,
                 File::success_t successCallback = File::defaultSuccessFunction,
                 File::failure_t failureCallback = File::defaultFailureFunction,
                 File::progress_t progressCallback = File::defaultProgressFunction);
    void addFile(char** servers, std::string filename,
                 File::success_t successCallback = File::defaultSuccessFunction,
                 File::failure_t failureCallback = File::defaultFailureFunction,
                 File::progress_t progressCallback = File::defaultProgressFunction);
    size_t count() const;
    void wait();
};
