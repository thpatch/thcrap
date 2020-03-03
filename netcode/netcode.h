#pragma once

#ifdef __cplusplus

#include <atomic>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cstdint>
#include <jansson.h>
#include <curl/curl.h>

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

class FileDownloading;
class File
{
private:
    // status is atomic, and can be changed quite often. We can change it with atomic operations.
    // Moving from a vector to another isn't guaranteed to be atomic, so we should rather use
    // a lock to move the thread-local vector to the shared one. This is done once per file,
    // when the download is complete, so this isn't performance-critical.
    std::string name;
    std::mutex data_mutex;
    std::vector<uint8_t> data;
    std::atomic<FileStatus> status;

    static size_t writeCallbackStatic(char *ptr, size_t size, size_t nmemb, void *userdata);
    size_t writeCallback(FileDownloading& fileDownloading, uint8_t *data, size_t size);
    // Set the File as downloading. Fails if the file is already downloaded.
    bool setDownloading();
    // Set the File as failed. Do nothing if another thread succeeded.
    void setFailed();

public:
    File(std::string name);
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    const std::string& getName() const;
    const std::vector<uint8_t>& getData() const;
    FileStatus getStatus() const;

    // If this function fails, the server is dead and should stop downloading files
    // (we'll fall back on another server).
    bool download(HttpHandle& http, const std::string& baseUrl);
};

class Repo;

class Server
{
private:
    std::string baseUrl;
    std::vector<std::thread> threads;

    void downloadThread(Repo& repo);

public:
    Server(std::string baseUrl);
    Server(const Server& src) = delete;
    Server(Server&& src);
    Server& operator=(const Server& src) = delete;

    // Download every file from the repo.
    // This function is asynchronous and will spawn between 1 and nbThreads threads.
    // After calling it, you must call the join() function.
    void startDownload(Repo& repo, unsigned int nbThreads);
    // Wait until the download session is finished.
    void join();
    // Download a single file from this server.
    std::unique_ptr<File> downloadFile(const std::string& name);
    // Download a single json file from this server.
    json_t *downloadJsonFile(const std::string& name);
};

class Repo
{
private:
    std::mutex mutex;

    std::vector<Server> servers;

    std::list<File> files;
    std::list<File*> todo;
    std::list<File*> downloading;
    std::list<File*> done;

public:
    // TODO: add a constructor that takes a repo.js and/or a files.js?
    Repo();
    // Create a repo from a single server.
    // Used during discovery.
    Repo(std::string serverUrl);
    ~Repo();
    Repo(const Repo& src) = delete;
    Repo& operator=(const Repo& src) = delete;

    // Download files.js for every patch in this repo
    // TODO (if we need it. I don't think we do.)
    json_t *downloadFilesJs();
    // Update every file for every patch in this repo.
    // TODO: add a version with a filter
    void download();

    // Take a file from the todo list.
    // The file is moved to the downloading list.
    File *takeFile();
    // Move a file from the downloading list to the done list.
    // If the file is already in the done list, nothing is done.
    // If the file isn't in the downloading list or in the done list,
    // this function throws an exception.
    void markFileAsDone(File *file);
};

extern "C" {
#endif // __cplusplus

// TODO: file write error callback
int RepoDiscoverAtURL(const char *start_url/*, file_write_error_t *fwe_callback*/);
int RepoDiscoverFromLocal(/*file_write_error_t *fwe_callback*/);

#ifdef __cplusplus
}
#endif
