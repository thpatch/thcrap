#include <functional>
#include "netcode.h"

Downloader::Downloader()
    : pool(8)
{}

Downloader::~Downloader()
{
    this->wait();
}

const File* Downloader::addFile(std::list<std::string> serversUrl, std::string filePath)
{
    std::list<DownloadUrl> urls;
    std::transform(serversUrl.begin(), serversUrl.end(), std::back_inserter(urls), [&filePath](const std::string& url) {
        auto [server, urlPath] = ServerCache::get().urlToServer(url + filePath);
        return DownloadUrl { .server = server, .url = urlPath };
    });
    
    this->files.emplace_back(std::move(urls));
    File& file = this->files.back();
    this->addToQueue(file);
    return &file;
}

void Downloader::addToQueue(File& file)
{
    file.decrementThreadsLeft();
    this->futuresList.push_back(this->pool.enqueue([&file]() {
        file.download();
    }));
}

void Downloader::wait()
{
    // For every file, run as many download threads as there is servers available.
    // The 2nd thread for a file will run only after every file have started once.
    bool loop = false;
    do {
        loop = false;
        for (File& file : this->files) {
            if (file.getThreadsLeft() > 0) {
                this->addToQueue(file);
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
