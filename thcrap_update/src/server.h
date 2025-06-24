#pragma once

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <jansson.h>
#include "file.h"
#include "thcrap_update_api.h"

class Server;
class BorrowedHttpHandle
{
private:
    std::unique_ptr<IHttpHandle> handle;
    Server& server;

public:
    BorrowedHttpHandle(std::unique_ptr<IHttpHandle> handle, Server& server);
    BorrowedHttpHandle(BorrowedHttpHandle&& src);
    ~BorrowedHttpHandle();
    BorrowedHttpHandle(const BorrowedHttpHandle& src) = delete;
    BorrowedHttpHandle& operator=(const BorrowedHttpHandle& src) = delete;

    IHttpHandle& operator*();
};


class Server
{
public:
    typedef std::function<std::unique_ptr<IHttpHandle>(void)> HttpHandleFactory;

private:
    HttpHandleFactory handleFactory;
    std::string baseUrl;
    // false if the server is dead (network timeout, 5XX error code, etc).
    std::atomic<bool> alive = true;
    std::mutex mutex;
    std::list<std::unique_ptr<IHttpHandle>> httpHandles;

public:
    Server(HttpHandleFactory handleFactory, std::string baseUrl);
    Server(const Server& src) = delete;
    Server(Server&& src) = delete;
    Server& operator=(const Server& src) = delete;

    bool isAlive() const;
    // Set this server as failed
    void fail();
    const std::string& getUrl() const;

    // Download a single file from this server.
    std::pair<std::vector<uint8_t>, HttpStatus> downloadFile(
        const std::string& name,
        File::progress_t progress = File::defaultProgressFunction
    );
    // Download a single json file from this server.
    std::pair<ScopedJson, HttpStatus> downloadJsonFile(const std::string& name, File::progress_t progress = File::defaultProgressFunction);

    // Borrow a HttpHandle from the server.
    // You own it until the BorrowedHttpHandle is destroyed.
    BorrowedHttpHandle borrowHandle();
    // Give the HttpHandle back to the server.
    // You should not call it, it is called automatically when the BorrowedHttpHandle
    // is destroyed.
    void giveBackHandle(std::unique_ptr<IHttpHandle> handle);
};

// Singleton
class ServerCache
{
private:
    static std::unique_ptr<ServerCache> instance;

    std::map<std::string, Server> cache;
    std::mutex mutex;
    Server::HttpHandleFactory httpHandleFactory = defaultHttpHandleFactory;

public:
    static THCRAP_UPDATE_API ServerCache& get();

    THCRAP_UPDATE_API ~ServerCache();

    // Clear the cache and free the objects in it.
    THCRAP_UPDATE_API void clear();
    // Set the HttpHandleFactory used for future servers.
    // The default factory should be fine for normal use, it defaults
    // to the factory defined in the build flags.
    // This is used for testing, to set fakes HttpHandles that will
    // test various success and error cases.
    THCRAP_UPDATE_API void setHttpHandleFactory(Server::HttpHandleFactory factory);
    // Default HttpHandleFactory for new servers.
    static THCRAP_UPDATE_API std::unique_ptr<IHttpHandle> defaultHttpHandleFactory();

    // Split an URL into an 'origin' part (protocol and domain name) and a 'path' part
    // (everything after the domain name).
    // The 'origin' part will be shared between every URL from the same server.
    std::pair<Server&, std::string> urlToServer(const std::string& url);

    // Find the server for url and call downloadFile on it
    std::pair<std::vector<uint8_t>, HttpStatus> downloadFile(
        const std::string& url,
        File::progress_t progress = File::defaultProgressFunction
    );
    // Find the server for url and call downloadJsonFile on it
    std::pair<ScopedJson, HttpStatus> downloadJsonFile(const std::string& url, File::progress_t progress = File::defaultProgressFunction);
};
