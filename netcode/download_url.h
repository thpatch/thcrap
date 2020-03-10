#pragma once

class Server;

class DownloadUrl
{
private:
    Server& server;
    std::string url;

public:
    DownloadUrl(Server& server, std::string url);
    DownloadUrl(const DownloadUrl& src);

    Server& getServer() const;
    // Return the full URL
    std::string getUrl() const;
};
