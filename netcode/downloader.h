#pragma once

#include <list>
#include <string>
#include <vector>
#include "ThreadPool.h"
#include "file.h"

class Downloader
{
private:
    typedef std::function<bool(const File&)> callback_t;
    ThreadPool pool;
    std::list<std::pair<File, callback_t>> files;
    std::vector<std::future<void>> futuresList;

    std::list<DownloadUrl> serversListToDownloadUrlList(const std::list<std::string>& serversUrl, const std::string& filePath);
    void addToQueue(std::pair<File, callback_t>& file);

public:
    Downloader();
    ~Downloader();
    const File* addFile(const std::list<std::string>& servers, std::string filename, callback_t successCallback = callback_t());
    // void addFilesJs(std::list<std::string> servers); // TODO
    void wait();
};
