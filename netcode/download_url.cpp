#include "server.h"
#include "download_url.h"

DownloadUrl::DownloadUrl(Server& server, std::string url)
    : server(server), url(std::move(url))
{}

DownloadUrl::DownloadUrl(const DownloadUrl& src)
    : server(src.server), url(src.url)
{}

Server& DownloadUrl::getServer() const
{
    return this->server;
}

std::string DownloadUrl::getUrl() const
{
    return this->server.getUrl() + this->url;
}
