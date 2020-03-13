#include "downloader.h"
#include "server.h"

Downloader::Downloader()
    : pool(8)
{}

Downloader::~Downloader()
{
    this->wait();
}

std::list<DownloadUrl> Downloader::serversListToDownloadUrlList(const std::list<std::string>& serversUrl, const std::string& filePath)
{
    std::list<DownloadUrl> urls;
    std::transform(serversUrl.begin(), serversUrl.end(), std::back_inserter(urls), [&filePath](const std::string& url) {
        auto [server, urlPath] = ServerCache::get().urlToServer(url + filePath);
        return DownloadUrl { .server = server, .url = urlPath };
    });
    return urls;
}

const File* Downloader::addFile(const std::list<std::string>& serversUrl, std::string filePath, Downloader::callback_t successCallback)
{
    std::list<DownloadUrl> urls = this->serversListToDownloadUrlList(serversUrl, filePath);
    this->files.emplace_back(std::move(urls), successCallback);
    auto& file = this->files.back();
    this->addToQueue(file);
    return &file.first;
}

void Downloader::addToQueue(std::pair<File, callback_t>& file)
{
    file.first.decrementThreadsLeft();
    this->futuresList.push_back(this->pool.enqueue([&file]() {
        if (file.first.download()) {
            // TODO: fail somewhere if this returns false
            file.second(file.first);
        }
    }));
}

void Downloader::wait()
{
    // For every file, run as many download threads as there is servers available.
    // The 2nd thread for a file will run only after every file have started once.
    bool loop = false;
    do {
        loop = false;
        for (auto& it : this->files) {
            File& file = it.first;
            if (file.getThreadsLeft() > 0) {
                this->addToQueue(it);
            }
            if (file.getThreadsLeft() > 0) {
                loop = true;
            }
        }
    } while (loop == true);

    for (auto& it : this->futuresList) {
        it.get();
    }
    this->futuresList.clear();
}
