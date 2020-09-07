#include "server.h"

std::unique_ptr<ServerCache> ServerCache::instance;


BorrowedHttpHandle::BorrowedHttpHandle(HttpHandle&& handle, Server& server)
    : handle(std::move(handle)), server(server), valid(true)
{}

BorrowedHttpHandle::BorrowedHttpHandle(BorrowedHttpHandle&& src)
    : handle(std::move(src.handle)), server(src.server), valid(src.valid)
{
    src.valid = false;
}

BorrowedHttpHandle::~BorrowedHttpHandle()
{
    if (this->valid) {
        this->server.giveBackHandle(std::move(this->handle));
    }
}

IHttpHandle& BorrowedHttpHandle::operator*()
{
    return this->handle;
}


Server::Server(std::string baseUrl)
    : baseUrl(std::move(baseUrl))
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

std::vector<uint8_t> Server::downloadFile(const std::string& name)
{
    std::list<DownloadUrl> urls {
        DownloadUrl(*this, name)
    };
    std::vector<uint8_t> ret;

    File file(std::move(urls), [&ret](const DownloadUrl&, std::vector<uint8_t>& data) {
        ret = std::move(data);
    });
    file.download();

    return ret;
}

json_t *Server::downloadJsonFile(const std::string& name)
{
    std::vector<uint8_t> data = this->downloadFile(name);
    json_t *json = json_loadb(reinterpret_cast<const char*>(data.data()), data.size(), 0, nullptr);
    return json;
}

BorrowedHttpHandle Server::borrowHandle()
{
    std::scoped_lock<std::mutex> lock(this->mutex);
    if (!this->httpHandles.empty()) {
        BorrowedHttpHandle handle(std::move(this->httpHandles.front()), *this);
        this->httpHandles.pop_front();
        return std::move(handle);
    }

    return BorrowedHttpHandle(std::move(HttpHandle()), *this);
}

void Server::giveBackHandle(HttpHandle&& handle)
{
    std::scoped_lock<std::mutex> lock(this->mutex);
    this->httpHandles.push_back(std::move(handle));
}


ServerCache& ServerCache::get()
{
    if (!ServerCache::instance) {
        ServerCache::instance = std::make_unique<ServerCache>();
    }
    return *ServerCache::instance;
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
            it = this->cache.emplace(origin, origin).first;
        }
        Server& server = it->second;
        return std::pair<Server&, std::string>(server, path);
    }
}

std::vector<uint8_t> ServerCache::downloadFile(const std::string& url)
{
    auto [server, path] = this->urlToServer(url);
    return server.downloadFile(path);
}

json_t *ServerCache::downloadJsonFile(const std::string& url)
{
    auto [server, path] = this->urlToServer(url);
    return server.downloadJsonFile(path);
}
