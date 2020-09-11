#include "thcrap.h"
#include "server.h"
#include "download_url.h"

DownloadUrl::DownloadUrl(Server& server, std::string url)
    : server(server), url(server.getUrl() + url)
{}

DownloadUrl::DownloadUrl(const DownloadUrl& src)
    : server(src.server), url(src.url)
{}

Server& DownloadUrl::getServer() const
{
    return this->server;
}

const std::string& DownloadUrl::getUrl() const
{
    return this->url;
}
