#include "downloader.h"
#include "server.h"

Downloader::Downloader()
    : pool(8), current_(0)
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
        return DownloadUrl(server, urlPath);
    });
    return urls;
}

void Downloader::addFile(const std::list<std::string>& serversUrl, std::string filePath,
                         File::success_t successCallback, File::failure_t failureCallback, File::progress_t progressCallback)
{
    std::list<DownloadUrl> urls = this->serversListToDownloadUrlList(serversUrl, filePath);
    this->files.emplace_back(std::move(urls), [successCallback, &current_ = this->current_](const DownloadUrl& url, std::vector<uint8_t>& data) {
        current_++;
        return successCallback(url, data);
    }, failureCallback, progressCallback);
    auto& file = this->files.back();
    this->addToQueue(file);
}

void Downloader::addFile(char** serversUrl, std::string filePath,
                         File::success_t successCallback, File::failure_t failureCallback, File::progress_t progressCallback)
{
    std::list<std::string> serversList;

    for (size_t i = 0; serversUrl && serversUrl[i]; i++) {
        serversList.push_back(serversUrl[i]);
    }
    this->addFile(serversList, filePath, successCallback, failureCallback, progressCallback);
}

void Downloader::addToQueue(File& file)
{
    file.decrementThreadsLeft();
    this->futuresList.push_back(this->pool.enqueue([&file]() {
        file.download();
    }));
}

size_t Downloader::current() const
{
    return this->current_;
}

size_t Downloader::total() const
{
    return this->files.size();
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
