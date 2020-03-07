#pragma once

#ifdef __cplusplus

#include <atomic>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cstdint>
#include <jansson.h>
#include <curl/curl.h>
#include "ThreadPool.h"

enum class FileStatus
{
    TODO,
    DOWNLOADING,
    DONE,
    FAILED
};

class HttpHandle
{
private:
    CURL *curl;

public:
    HttpHandle();
    HttpHandle(HttpHandle&& other);
    HttpHandle(HttpHandle& other) = delete;
    HttpHandle& operator=(HttpHandle& other) = delete;
    ~HttpHandle();
    CURL *operator*();
};

class Server;
class BorrowedHttpHandle
{
private:
    HttpHandle handle;
    Server& server;
    bool valid;

public:
    BorrowedHttpHandle(HttpHandle&& handle, Server& server);
    BorrowedHttpHandle(BorrowedHttpHandle&& src);
    ~BorrowedHttpHandle();
    BorrowedHttpHandle(const BorrowedHttpHandle& src) = delete;
    BorrowedHttpHandle& operator=(const BorrowedHttpHandle& src) = delete;

    HttpHandle& operator*();
};

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

// TODO: move the cURL code to HttpHandle
class FileDownloading;
class File
{
private:
    // status is atomic, and can be changed quite often. We can change it with atomic operations.
    // Moving from a vector to another isn't guaranteed to be atomic, so we should rather use
    // a lock to move the thread-local vector to the shared one. This is done once per file,
    // when the download is complete, so this isn't performance-critical.
    //std::string name;
    std::mutex mutex;
    std::vector<uint8_t> data;
    std::atomic<FileStatus> status;
    // Number of threads left to enqueue into the thread pool.
    // It is tightly coupled with the number of servers, but may be lower.
    // threadsLeft will decrease when we enqueue a download into the thread pool,
    // but servers.size() will decrease only when the thread pool picks our download
    // and starts it.
    std::list<DownloadUrl> urls;
    unsigned int threadsLeft;

    static size_t writeCallbackStatic(char *ptr, size_t size, size_t nmemb, void *userdata);
    size_t writeCallback(FileDownloading& fileDownloading, uint8_t *data, size_t size);
    // Set the File as downloading. Fails if the file is already downloaded.
    bool setDownloading();
    // Set the File as failed. Do nothing if another thread succeeded.
    void setFailed();
    DownloadUrl pickUrl();
    // If this function fails, the server is dead.
    bool download(HttpHandle& http, const std::string& url);

public:
    // TODO
    File(std::list<DownloadUrl>&& urls);
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    const std::vector<uint8_t>& getData() const;
    FileStatus getStatus() const;
    unsigned int getThreadsLeft() const;
    void decrementThreadsLeft();

    void download();
};

class Repo;

class Server
{
private:
    std::string baseUrl;
    //std::vector<std::thread> threads;
    // false if the server have proven to be unreliable or dead
    // (network timeout, a file doesn't match its CRC, etc).
    // TODO: set to false on failure
    std::atomic<bool> alive = true;
    std::mutex mutex;
    std::list<HttpHandle> httpHandles;

    void downloadThread(Repo& repo);

public:
    Server(std::string baseUrl);
    Server(const Server& src) = delete;
    Server(Server&& src) = delete;
    Server& operator=(const Server& src) = delete;

    // TODO
    bool isAlive() const;
    // Set this server as failed
    void fail();
    const std::string& getUrl() const;

    // Download a single file from this server.
    std::unique_ptr<File> downloadFile(const std::string& name);
    // Download a single json file from this server.
    json_t *downloadJsonFile(const std::string& name);

    // Borrow a HttpHandle from the server.
    // You own it until the BorrowedHttpHandle is destroyed.
    BorrowedHttpHandle borrowHandle();
    // Give the HttpHandle back to the server.
    // You should not call it, it is called automatically when the BorrowedHttpHandle
    // is destroyed.
    void giveBackHandle(HttpHandle&& handle);
};

// Singleton
class ServerCache
{
private:
    static std::unique_ptr<ServerCache> instance;

    std::map<std::string, Server> cache;
    std::mutex mutex;

public:
    static ServerCache& get();

    // Split an URL into an 'origin' part (protocol and domain name) and a 'path' part
    // (everything after the domain name).
    // The 'origin' part will be shared between every URL from the same server.
    std::pair<Server&, std::string> urlToServer(const std::string& url);

    // Find the server for url and call downloadFile/downloadJsonFile on it
    std::unique_ptr<File> downloadFile(const std::string& url);
    json_t *downloadJsonFile(const std::string& url);
};

class Downloader
{
private:
    ThreadPool pool;
    std::list<File> files;
    std::vector<std::future<void>> futuresList;

    void addToQueue(File& file);

public:
    Downloader();
    ~Downloader();
    const File* addFile(std::list<std::string> servers, std::string filename);
    void wait();
};

extern "C" {
#endif // __cplusplus

// TODO: file write error callback
int RepoDiscoverAtURL(const char *start_url/*, file_write_error_t *fwe_callback*/);
int RepoDiscoverFromLocal(/*file_write_error_t *fwe_callback*/);

#ifdef __cplusplus
}
#endif
