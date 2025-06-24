#include "thcrap.h"
#include "server.h"

#include "http_curl.h"
#include "http_wininet.h"

std::unique_ptr<ServerCache> ServerCache::instance;


BorrowedHttpHandle::BorrowedHttpHandle(std::unique_ptr<IHttpHandle> handle, Server& server)
    : handle(std::move(handle)), server(server)
{
}

BorrowedHttpHandle::BorrowedHttpHandle(BorrowedHttpHandle&& src)
    : handle(std::move(src.handle)), server(src.server)
{
}

BorrowedHttpHandle::~BorrowedHttpHandle()
{
    if (this->handle) {
        this->server.giveBackHandle(std::move(this->handle));
    }
}

IHttpHandle& BorrowedHttpHandle::operator*()
{
    return *this->handle;
}


Server::Server(HttpHandleFactory handleFactory, std::string baseUrl)
    : handleFactory(handleFactory), baseUrl(std::move(baseUrl))
{
    if (this->baseUrl[this->baseUrl.length() - 1] != '/') {
        this->baseUrl.append("/");
    }
}

bool Server::isAlive() const
{
    return this->alive;
}

void Server::fail()
{
    this->alive = false;
}

const std::string& Server::getUrl() const
{
    return this->baseUrl;
}

std::pair<std::vector<uint8_t>, HttpStatus> Server::downloadFile(const std::string& name, File::progress_t progress)
{
    std::list<DownloadUrl> urls{
        DownloadUrl(*this, name)
    };
    std::vector<uint8_t> ret;
    HttpStatus status = HttpStatus::makeOk();

    File file(std::move(urls),
        [&ret](const DownloadUrl&, std::vector<uint8_t>& data) {
            ret = std::move(data);
        },
        [&status](const DownloadUrl&, HttpStatus error) {
            status = error;
        },
        progress
    );
    file.download();

    return std::make_pair(std::move(ret), status);
}

std::pair<ScopedJson, HttpStatus> Server::downloadJsonFile(const std::string& name, File::progress_t progress)
{
    auto [data, status] = this->downloadFile(name, progress);
    if (!status) {
        return std::make_pair(nullptr, status);
    }

    ScopedJson json = json5_loadb(data.data(), data.size(), nullptr);
    return std::make_pair(json, status);
}

BorrowedHttpHandle Server::borrowHandle()
{
    std::scoped_lock<std::mutex> lock(this->mutex);
    if (!this->httpHandles.empty()) {
        BorrowedHttpHandle handle(std::move(this->httpHandles.front()), *this);
        this->httpHandles.pop_front();
        return handle;
    }

    return BorrowedHttpHandle(this->handleFactory(), *this);
}

void Server::giveBackHandle(std::unique_ptr<IHttpHandle> handle)
{
    std::scoped_lock<std::mutex> lock(this->mutex);
    this->httpHandles.push_back(std::move(handle));
}


std::unique_ptr<IHttpHandle> ServerCache::defaultHttpHandleFactory()
{
    static enum {
        WININET_NOT_INITIALIZED = -1,
        WININET_FALSE = 0,
        WININET_TRUE = 1,
    } use_wininet = WININET_NOT_INITIALIZED;

    if (use_wininet == WININET_NOT_INITIALIZED) {
        use_wininet = globalconfig_get_boolean("use_wininet", false) ? WININET_TRUE : WININET_FALSE;
        if (use_wininet) {
            log_print("Update: using WinINet backend\n");
        }
        else {
            log_print("Update: using libcurl backend\n");
        }
    }

    if (use_wininet) {
        return std::make_unique<WininetHandle>();
    }
    else {
        return std::make_unique<CurlHandle>();
    }
}

ServerCache::~ServerCache()
{
}

void ServerCache::setHttpHandleFactory(Server::HttpHandleFactory factory)
{
    this->httpHandleFactory = factory;
}

ServerCache& ServerCache::get()
{
    if (!ServerCache::instance) {
        ServerCache::instance = std::make_unique<ServerCache>();
    }
    return *ServerCache::instance;
}

void ServerCache::clear()
{
    this->cache.clear();
}

std::pair<Server&, std::string> ServerCache::urlToServer(const std::string& url)
{
    size_t pos = url.find("://");
    if (pos != std::string::npos) {
        pos += 3;
    }
    else {
        pos = 0;
    }
    pos = url.find('/', pos);

    std::string origin;
    std::string path;
    if (pos != std::string::npos) {
        origin = url.substr(0, pos + 1);
        if (url.length() > pos + 1) {
            path = url.substr(pos + 1, std::string::npos);
        }
    }
    else {
        origin = url;
    }

    {
        std::scoped_lock<std::mutex> lock(this->mutex);
        auto it = this->cache.find(origin);
        if (it == this->cache.end()) {
            it = this->cache.emplace(std::piecewise_construct,
                std::forward_as_tuple(origin),
                std::forward_as_tuple(this->httpHandleFactory, origin)).first;
        }
        Server& server = it->second;
        return std::pair<Server&, std::string>(server, path);
    }
}

std::pair<std::vector<uint8_t>, HttpStatus> ServerCache::downloadFile(const std::string& url, File::progress_t progress)
{
    auto [server, path] = this->urlToServer(url);
    return server.downloadFile(path, progress);
}

std::pair<ScopedJson, HttpStatus> ServerCache::downloadJsonFile(const std::string& url, File::progress_t progress)
{
    auto [server, path] = this->urlToServer(url);
    return server.downloadJsonFile(path, progress);
}
