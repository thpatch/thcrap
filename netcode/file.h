#pragma once

#include <atomic>
#include <list>
#include <mutex>
#include <vector>
#include "http.h"

class Server;
struct DownloadUrl
{
    Server& server;
    std::string url;
    DownloadUrl(Server& server, std::string url)
        : server(server), url(std::move(url))
    {}
    DownloadUrl(const DownloadUrl& src)
        : server(src.server), url(src.url)
    {}
};

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
    unsigned int threadsLeft;

    size_t writeCallback(std::vector<uint8_t>& buffer, const uint8_t *data, size_t size);
    bool setDownloading();
    void setFailed();
    DownloadUrl pickUrl();
    // If this function fails, the server is dead.
    bool download(HttpHandle& http, const std::string& url);

public:
    File(std::list<DownloadUrl>&& urls);
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    const std::vector<uint8_t>& getData() const;
    FileStatus getStatus() const;
    unsigned int getThreadsLeft() const;
    void decrementThreadsLeft();

    void download();
};
