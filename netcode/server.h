#pragma once

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <jansson.h>
#include "file.h"
#include "http.h"

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


class Server
{
private:
    std::string baseUrl;
    // false if the server have proven to be unreliable or dead
    // (network timeout, a file doesn't match its CRC, etc).
    // TODO: set to false on failure
    std::atomic<bool> alive = true;
    std::mutex mutex;
    std::list<HttpHandle> httpHandles;

public:
    Server(std::string baseUrl);
    Server(const Server& src) = delete;
    Server(Server&& src) = delete;
    Server& operator=(const Server& src) = delete;

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
