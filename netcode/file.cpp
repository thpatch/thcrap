#include "netcode.h"

// Structure for thread-local download data
struct FileDownloading
{
    File& file;
    std::vector<uint8_t> local_data;
};

File::File(std::list<DownloadUrl>&& urls)
    : status(FileStatus::TODO), urls(urls), threadsLeft(urls.size())
{}

const std::vector<uint8_t>& File::getData() const
{
    return this->data;
}

FileStatus File::getStatus() const
{
    return this->status;
}

unsigned int File::getThreadsLeft() const
{
    return this->threadsLeft;
}

void File::decrementThreadsLeft()
{
    if (this->threadsLeft == 0) {
        throw std::logic_error("Trying to decrement File::threadsLeft, but it is already 0");
    }
    this->threadsLeft--;
}

size_t File::writeCallback(std::vector<uint8_t>& buffer, const uint8_t *data, size_t size)
{
    if (this->status == FileStatus::DONE) {
        // Any value different from size signals an error to libcurl
        return 0;
    }
    buffer.insert(buffer.end(), data, data + size);
    return size;
}

bool File::setDownloading()
{
    FileStatus todo = FileStatus::TODO;
    if (this->status.compare_exchange_strong(todo, FileStatus::DOWNLOADING)) {
        return true;
    }
    else if (this->status == FileStatus::DOWNLOADING) {
        return true;
    }
    else {
        return false;
    }
}

void File::setFailed()
{
    // We can't be in todo because we alreadu started a download.
    // If we're already in failed, we don't need to change the status.
    // If we're in done, we don't *want* to change the status.
    // So we only need to change from downloading to failed.
    FileStatus downloading = FileStatus::DOWNLOADING;
    this->status.compare_exchange_strong(downloading, FileStatus::FAILED);
}

bool File::download(HttpHandle& http, const std::string& url)
{
    if (!this->setDownloading()) {
        return true;
    }
    printf("Starting %s...\n", url.c_str());

    std::vector<uint8_t> localData;
    if (!http.download(url, [this, &localData](const uint8_t *data, size_t size) {
        return this->writeCallback(localData, data, size);
    })) {
        this->setFailed();
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(this->mutex);
        if (this->status == FileStatus::DOWNLOADING || // We're the first thread to finish downloading the file
            this->status == FileStatus::FAILED) {      // Another thread failed to download the file, but this thread succeeded
            this->data = std::move(localData);
            this->status = FileStatus::DONE;
        }
        // else, do nothing. Another thread finished before us.
    }
    return true;
}

DownloadUrl File::pickUrl()
{
    std::scoped_lock<std::mutex> lock(this->mutex);
    // TODO: srand (actually we don't really care about having a good random source)
    int n = rand() % this->urls.size();
    auto it = this->urls.begin();
    std::advance(it, n);
    DownloadUrl url = *it;
    this->urls.erase(it);
    return url;
}

void File::download()
{
    DownloadUrl url = this->pickUrl();
    BorrowedHttpHandle handle = url.server.borrowHandle();
    if (!this->download(*handle, url.server.getUrl() + url.url)) {
        url.server.fail();
    }
}
