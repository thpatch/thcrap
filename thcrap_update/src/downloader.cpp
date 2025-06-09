#include "thcrap.h"
#include "downloader.h"
#include "server.h"

Downloader::Downloader()
    : pool(8), current_(0), total_(0)
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
                         File::success_t successCallback, File::failure_t failureCallback, File::progress_t progressCallback,
						 DownloadCache *cache)
{
    std::scoped_lock lock(this->mutex);

    std::list<DownloadUrl> urls = this->serversListToDownloadUrlList(serversUrl, filePath);
    auto successLambda = [successCallback, &current_ = this->current_](const DownloadUrl& url, std::vector<uint8_t>& data) {
        current_++;
        return successCallback(url, data);
    };
    std::shared_ptr<File> file = std::make_unique<File>(std::move(urls), successLambda, failureCallback, progressCallback, cache);

    this->futuresList.push_back(this->pool.enqueue([file]() {
        file->download();
    }));

    this->total_++;
}

void Downloader::addFile(char** serversUrl, std::string filePath,
                         File::success_t successCallback, File::failure_t failureCallback, File::progress_t progressCallback,
						 DownloadCache *cache)
{
    std::list<std::string> serversList;

    for (size_t i = 0; serversUrl && serversUrl[i]; i++) {
        serversList.push_back(serversUrl[i]);
    }
    this->addFile(serversList, filePath, successCallback, failureCallback, progressCallback, cache);
}

size_t Downloader::current() const
{
    return this->current_;
}

size_t Downloader::total() const
{
    return this->total_;
}

void Downloader::wait()
{
    for (auto& it : this->futuresList) {
        it.get();
    }
    this->futuresList.clear();
}
